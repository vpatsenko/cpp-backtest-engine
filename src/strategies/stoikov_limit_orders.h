#pragma once

#include "../core/strategy.h"
#include "../utils/volatility_estimator.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <optional>

namespace backtest {

// True Stoikov Market Making Strategy with Limit Orders
// Posts limit orders at theoretical bid/ask and earns the spread
class StoikovLimitOrderStrategy : public IStrategy {
public:
    struct ActiveQuote {
        int order_id;
        Side side;
        double price;
        double quantity;
    };

    StoikovLimitOrderStrategy(
        double gamma = 0.05,              // Risk aversion parameter (γ in paper)
        double kappa = 2.0,                // Order arrival rate (κ in paper)
        double time_horizon = 300.0,       // Initial time horizon in seconds (T in paper)
        double order_quantity = 15000.0,   // Base order size
        double max_inventory = 100000.0,   // Maximum inventory
        double min_spread = 0.00001,       // Minimum spread (practical floor, not in paper)
        double refresh_threshold = 0.0002  // Refresh if mid moves > threshold
    )
        : gamma_(gamma)
        , kappa_(kappa)
        , initial_time_horizon_(time_horizon)
        , order_quantity_(order_quantity)
        , max_inventory_(max_inventory)
        , min_spread_(min_spread)
        , refresh_threshold_(refresh_threshold)
        , inventory_(0.0)
        , tick_count_(0)
        , last_mid_price_(0.0)
        , start_time_(0)
        , volatility_estimator_(1000)
    {}

    void on_book_update(uint64_t timestamp, const MarketState& market) override {
        tick_count_++;

        // Initialize start time on first update
        if (start_time_ == 0) {
            start_time_ = timestamp;
        }

        double mid_price = market.mid_price();
        if (mid_price <= 0.0) return;

        // Update volatility estimate
        volatility_estimator_.update(mid_price);

        // Only refresh quotes every 100 ticks to reduce noise
        if (tick_count_ % 100 != 0) return;

        // Check if we need to refresh quotes
        if (!should_refresh(mid_price)) return;

        // Calculate time remaining: (T - t) in seconds
        double elapsed_seconds = (timestamp - start_time_) / 1e6;  // Convert microseconds to seconds
        double time_remaining = std::max(1.0, initial_time_horizon_ - elapsed_seconds);

        // Get volatility estimate (annualized)
        double sigma = volatility_estimator_.get_volatility();

        // Convert annualized volatility to per-second volatility in price units
        // sigma is annualized (e.g., 0.5 = 50% per year)
        // We need σ in price units per second
        double seconds_per_year = 31536000.0;  // 365.25 * 24 * 3600
        double sigma_per_second = (mid_price * sigma) / std::sqrt(seconds_per_year);
        double sigma_squared = sigma_per_second * sigma_per_second;

        // AVELLANEDA-STOIKOV MODEL FORMULAS:

        // 1. Reservation price: r(s, q, t) = s - q·γ·σ²·(T-t)
        //    This is the indifference price given current inventory
        double reservation_price = mid_price - (inventory_ * gamma_ * sigma_squared * time_remaining);

        // 2. Optimal spread: δ* = γ·σ²·(T-t) + (2/γ)·ln(1 + γ/κ)
        //    First term: inventory risk component
        //    Second term: order flow/liquidity component
        double spread_inventory_term = gamma_ * sigma_squared * time_remaining;
        double spread_liquidity_term = (2.0 / gamma_) * std::log(1.0 + gamma_ / kappa_);
        double delta = spread_inventory_term + spread_liquidity_term;

        // Practical adjustments (not in theoretical paper):
        // - Floor at min_spread to avoid crossing quotes
        // - Cap at reasonable percentage to avoid unrealistic quotes
        delta = std::max(delta, min_spread_ * 2.0);
        delta = std::min(delta, mid_price * 0.02);  // Cap at 2% of mid

        // Debug output (first time only)
        if (tick_count_ == 100) {
            std::cout << "A-S Model Debug:\n";
            std::cout << "  mid=" << mid_price << " σ_annual=" << sigma
                      << " σ²=" << sigma_squared << "\n";
            std::cout << "  inventory=" << inventory_ << " time_remaining=" << time_remaining << "s\n";
            std::cout << "  reservation_price=" << reservation_price << "\n";
            std::cout << "  spread: inventory_term=" << spread_inventory_term
                      << " liquidity_term=" << spread_liquidity_term
                      << " total=" << delta << "\n";
        }

        // 3. Optimal quotes: bid = r - δ/2, ask = r + δ/2
        double target_bid = reservation_price - delta / 2.0;
        double target_ask = reservation_price + delta / 2.0;

        // Inventory skewing: adjust quote sizes based on inventory
        double inv_ratio = inventory_ / max_inventory_;
        double bid_quantity = order_quantity_ * (1.0 - inv_ratio);
        double ask_quantity = order_quantity_ * (1.0 + inv_ratio);

        if (max_inventory_ > 0) {
            bid_quantity = std::max(bid_quantity, 1000.0);
            ask_quantity = std::max(ask_quantity, 1000.0);
        }

        // Ensure bid < ask (no crossing)
        if (target_bid >= target_ask) {
            target_bid = mid_price - delta / 2.0;
            target_ask = mid_price + delta / 2.0;
        }

        // Refresh quotes
        refresh_quotes(target_bid, target_ask, bid_quantity, ask_quantity);

        last_mid_price_ = mid_price;
    }

    void on_trade(uint64_t timestamp, const TradeEvent& trade) override {
        // Not used in this strategy
        (void)timestamp;
        (void)trade;
    }

    void on_fill(uint64_t timestamp, const Fill& fill) override {
        // Update inventory
        int64_t sign = static_cast<int64_t>(fill.side);
        inventory_ += fill.quantity * sign;

        std::cout << "Fill at " << timestamp << ": "
                  << (fill.side == Side::BUY ? "BUY" : "SELL")
                  << " " << fill.quantity << " @ " << fill.price
                  << " (Order #" << fill.order_id << ")"
                  << " | Inventory: " << inventory_ << "\n";

        // Clear filled order from tracking
        if (active_bid_.has_value() && fill.order_id == active_bid_->order_id) {
            active_bid_.reset();
        }
        if (active_ask_.has_value() && fill.order_id == active_ask_->order_id) {
            active_ask_.reset();
        }
    }

    void on_order_submitted(int order_id, const Order& order) override {
        // Store order ID with quote tracking
        if (order.side == Side::BUY) {
            active_bid_ = ActiveQuote{order_id, order.side, order.price, order.quantity};
            std::cout << "Posted BID #" << order_id << ": " << order.quantity
                      << " @ " << order.price << "\n";
        } else {
            active_ask_ = ActiveQuote{order_id, order.side, order.price, order.quantity};
            std::cout << "Posted ASK #" << order_id << ": " << order.quantity
                      << " @ " << order.price << "\n";
        }
    }

private:
    bool should_refresh(double current_mid) {
        // Refresh if missing quotes
        if (!active_bid_.has_value() || !active_ask_.has_value()) {
            return true;
        }

        // Refresh if market moved significantly
        if (last_mid_price_ > 0.0) {
            double mid_move = std::abs(current_mid - last_mid_price_) / last_mid_price_;
            if (mid_move > refresh_threshold_) {
                return true;
            }
        }

        return false;
    }

    void refresh_quotes(double bid_price, double ask_price, double bid_qty, double ask_qty) {
        // Cancel existing orders
        if (active_bid_.has_value()) {
            cancel_order(active_bid_->order_id);
            active_bid_.reset();
        }
        if (active_ask_.has_value()) {
            cancel_order(active_ask_->order_id);
            active_ask_.reset();
        }

        // Submit new limit orders (both in same event - enabled by framework changes!)
        Order bid_order;
        bid_order.side = Side::BUY;
        bid_order.price = bid_price;
        bid_order.quantity = bid_qty;
        submit_order(bid_order);

        Order ask_order;
        ask_order.side = Side::SELL;
        ask_order.price = ask_price;
        ask_order.quantity = ask_qty;
        submit_order(ask_order);
    }

    // Model parameters (Avellaneda-Stoikov)
    double gamma_;              // Risk aversion parameter (γ)
    double kappa_;              // Order arrival rate (κ)
    double initial_time_horizon_;  // Initial time horizon T (seconds)

    // Practical parameters (not in paper)
    double order_quantity_;     // Base order size
    double max_inventory_;      // Maximum inventory
    double min_spread_;         // Minimum spread (floor)
    double refresh_threshold_;  // Refresh threshold

    // State
    double inventory_;          // Current inventory (q)
    size_t tick_count_;
    double last_mid_price_;
    uint64_t start_time_;       // Strategy start time for (T-t) calculation
    std::optional<ActiveQuote> active_bid_;
    std::optional<ActiveQuote> active_ask_;

    // Components
    VolatilityEstimator volatility_estimator_;
};

} // namespace backtest
