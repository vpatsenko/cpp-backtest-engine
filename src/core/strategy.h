#pragma once

#include "types.h"
#include "market_state.h"
#include <vector>

namespace backtest {

class IStrategy {
public:
    virtual ~IStrategy() = default;

    virtual void on_book_update(uint64_t timestamp, const MarketState& market) = 0;
    virtual void on_trade(uint64_t timestamp, const TradeEvent& trade) = 0;
    virtual void on_fill(uint64_t timestamp, const Fill& fill) = 0;
    virtual void on_order_submitted(int order_id, const Order& order) {}

    // Strategy can emit orders
    bool has_pending_orders() const { return !pending_orders_.empty(); }
    std::vector<Order> take_pending_orders() {
        auto orders = std::move(pending_orders_);
        pending_orders_.clear();
        return orders;
    }

    // Strategy can emit order cancellations
    bool has_pending_cancellations() const { return !pending_cancellations_.empty(); }
    std::vector<int> take_pending_cancellations() {
        auto result = pending_cancellations_;
        pending_cancellations_.clear();
        return result;
    }

protected:
    void submit_order(const Order& order) {
        pending_orders_.push_back(order);
    }

    void cancel_order(int order_id) {
        pending_cancellations_.push_back(order_id);
    }

private:
    std::vector<Order> pending_orders_;
    std::vector<int> pending_cancellations_;
};

} // namespace backtest
