#pragma once

#include "../core/strategy.h"
#include "../utils/volatility_estimator.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace backtest {

// Simplified Stoikov Market Making Strategy
// Uses market orders to approximate market making behavior
class StoikovMarketMakingStrategy : public IStrategy {
public:
    StoikovMarketMakingStrategy(
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

        // Only trade every 100 ticks to reduce noise
        if (tick_count_ % 100 != 0) return;

        // Calculate reservation price
        double sigma = volatility_estimator_.get_volatility();
        double time_remaining = time_horizon_;

        // Reservation price: r = mid - q * gamma * sigma^2 * T
        double reservation_price = mid_price - (inventory_ * gamma_ * sigma * sigma * time_remaining);

        // Optimal spread: delta = (1/gamma) * ln(1 + gamma/kappa)
        double delta_val = (1.0 / gamma_) * std::log(1.0 + gamma_ / kappa_);
        delta_val = std::max(delta_val, min_spread_ * 3.0);  // Ensure reasonable spread
        double delta = delta_val;

        // Calculate target bid and ask prices (our theoretical quotes)
        double target_bid = reservation_price - delta / 2.0;
        double target_ask = reservation_price + delta / 2.0;

        // Get current market quotes
        double market_best_bid = market.best_bid();
        double market_best_ask = market.best_ask();

        // Inventory skewing: trade to reduce inventory
        double inv_ratio = inventory_ / max_inventory_;
        double bid_quantity = order_quantity_ * (1.0 - inv_ratio);
        double ask_quantity = order_quantity_ * (1.0 + inv_ratio);

        if (max_inventory_ > 0) {
            bid_quantity = std::max(bid_quantity, 1000.0);
            ask_quantity = std::max(ask_quantity, 1000.0);
        }

        // Market making logic with market orders:
        // - Buy if market ask is below our target ask (cheap offer available)
        // - Sell if market bid is above our target bid (good bid available)

        if (market_best_ask > 0.0 && market_best_ask < target_ask && inventory_ < max_inventory_) {
            // Market is offering to sell below our target ask - buy it
            Order buy_order;
            buy_order.side = Side::BUY;
            buy_order.quantity = bid_quantity;
            buy_order.price = 0.0;  // Market order
            submit_order(buy_order);
        }
        else if (market_best_bid > 0.0 && market_best_bid > target_bid && inventory_ > -max_inventory_) {
            // Market is bidding above our target bid - sell to it
            Order sell_order;
            sell_order.side = Side::SELL;
            sell_order.quantity = ask_quantity;
            sell_order.price = 0.0;  // Market order
            submit_order(sell_order);
        }

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
                  << " | Inventory: " << inventory_ << "\n";
    }

private:
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

    // Components
    VolatilityEstimator volatility_estimator_;
};

} // namespace backtest
