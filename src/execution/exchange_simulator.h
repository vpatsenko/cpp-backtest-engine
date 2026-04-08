#pragma once

#include "../core/types.h"
#include "../core/market_state.h"
#include <vector>

namespace backtest {

// Exchange simulator - maintains order book and matches limit orders
class ExchangeSimulator {
public:
    ExchangeSimulator() = default;

    // Market data updates (called by ReplayEngine on each book update)
    void onBookUpdate(const BookUpdateEvent& event, uint64_t timestamp);

    // Order management API (called by ReplayEngine when strategy submits orders)
    int submitLimitOrder(const Order& order, uint64_t timestamp);
    void cancelOrder(int order_id);

    // Fill retrieval (called by ReplayEngine to get generated fills)
    std::vector<Fill> popFills();
    bool hasPendingFills() const { return !pending_fills_.empty(); }

    // Diagnostics
    size_t pendingOrderCount() const { return buy_orders_.size() + sell_orders_.size(); }

    // Access to current market state
    const MarketState& market() const { return market_; }

private:
    // Pending limit order with tracking info
    struct PendingOrder {
        int id;
        Order order;
        uint64_t submit_timestamp;
        double filled_quantity;

        PendingOrder(int id_, const Order& o, uint64_t ts)
            : id(id_), order(o), submit_timestamp(ts), filled_quantity(0.0) {}

        double remaining_quantity() const {
            return order.quantity - filled_quantity;
        }
    };

    // Core matching logic
    void checkCrosses(uint64_t timestamp);
    std::vector<Fill> executeOrder(PendingOrder& pending, uint64_t timestamp);

    // Internal state
    MarketState market_;
    std::vector<PendingOrder> buy_orders_;
    std::vector<PendingOrder> sell_orders_;
    std::vector<Fill> pending_fills_;
    int next_order_id_ = 1;
};

} // namespace backtest
