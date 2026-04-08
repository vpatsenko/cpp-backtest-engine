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
        double gamma = 0.05,              // Risk aversion parameter
        double kappa = 2.0,                // Order arrival rate
        double time_horizon = 300.0,       // Time horizon in seconds
        double order_quantity = 15000.0,   // Base order size
        double max_inventory = 100000.0,   // Maximum inventory
        double min_spread = 0.00001,       // Minimum spread (1 tick)
        double refresh_threshold = 0.0002  // Refresh if mid moves > 2bp
    )
        : gamma_(gamma)
        , kappa_(kappa)
        , time_horizon_(time_horizon)
        , order_quantity_(order_quantity)
        , max_inventory_(max_inventory)
        , min_spread_(min_spread)
        , refresh_threshold_(refresh_threshold)
        , inventory_(0.0)
        , tick_count_(0)
        , last_mid_price_(0.0)
        , volatility_estimator_(1000)
    {}

    void on_book_update(uint64_t timestamp, const MarketState& market) override {
        (void)timestamp;
        tick_count_++;

        double mid_price = market.mid_price();
        if (mid_price <= 0.0) return;

        // Update volatility estimate
        volatility_estimator_.update(mid_price);

        // Only refresh quotes every 100 ticks to reduce noise
        if (tick_count_ % 100 != 0) return;

        // Check if we need to refresh quotes
        if (!should_refresh(mid_price)) return;

        // Calculate Stoikov quotes
        double sigma = volatility_estimator_.get_volatility();
        double time_remaining = time_horizon_;

        // Convert annualized volatility to per-second volatility in price units
        // sigma is annualized (e.g., 0.5 = 50% per year)
        // We need sigma in price units per second
        double seconds_per_year = 31536000.0;  // 365.25 * 24 * 3600
        double sigma_per_second = (mid_price * sigma) / std::sqrt(seconds_per_year);

        // Reservation price: r = mid - q * gamma * sigma^2 * T
        double reservation_price = mid_price - inventory_ * gamma_ * sigma_per_second * sigma_per_second * time_remaining;

        // Debug output (first time only)
        if (tick_count_ == 100) {
            std::cout << "Debug: mid=" << mid_price << " sigma_annual=" << sigma
                      << " sigma_per_sec=" << sigma_per_second << " inv=" << inventory_
                      << " res_price=" << reservation_price << "\n";
        }

        // Optimal spread: delta = (1/gamma) * ln(1 + gamma/kappa)
        // Scale spread to be reasonable relative to price (theory gives absolute values)
        double delta_bps = (1.0 / gamma_) * std::log(1.0 + gamma_ / kappa_);  // In basis points
        double delta = mid_price * delta_bps * 0.0001;  // Convert to price units
        delta = std::max(delta, min_spread_ * 2.0);  // Ensure minimum spread
        delta = std::min(delta, mid_price * 0.02);   // Cap at 2% of mid price

        // Calculate target bid and ask prices
        double target_bid = reservation_price - delta / 2.0;
        double target_ask = reservation_price + delta / 2.0;

        // Inventory skewing: adjust quote sizes based on inventory
        double bid_quantity = order_quantity_;
        double ask_quantity = order_quantity_;

        if (max_inventory_ > 0) {
            double inventory_ratio = inventory_ / max_inventory_;
            bid_quantity = order_quantity_ * (1.0 - inventory_ratio);
            ask_quantity = order_quantity_ * (1.0 + inventory_ratio);
        }

        bid_quantity = std::max(bid_quantity, 1000.0);
        ask_quantity = std::max(ask_quantity, 1000.0);

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
        int sign = static_cast<int>(fill.side);
        inventory_ += sign * fill.quantity;

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

    // Parameters
    double gamma_;              // Risk aversion
    double kappa_;              // Arrival rate
    double time_horizon_;       // Time horizon (seconds)
    double order_quantity_;     // Base order size
    double max_inventory_;      // Maximum inventory
    double min_spread_;         // Minimum spread
    double refresh_threshold_;  // Refresh threshold

    // State
    double inventory_;
    size_t tick_count_;
    double last_mid_price_;
    std::optional<ActiveQuote> active_bid_;
    std::optional<ActiveQuote> active_ask_;

    // Components
    VolatilityEstimator volatility_estimator_;
};

} // namespace backtest
