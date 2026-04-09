#include "exchange_simulator.h"
#include <algorithm>
#include <iostream>

namespace backtest {

// Update market state and check for order crosses
void ExchangeSimulator::onBookUpdate(const BookUpdateEvent& event, uint64_t timestamp) {
    // Update internal market state
    market_.apply(event);

    // Check if any pending orders can execute
    checkCrosses(timestamp);
}

// Submit a new limit order to the exchange
int ExchangeSimulator::submitLimitOrder(const Order& order, uint64_t timestamp) {
    // Validate order
    if (order.is_market()) {
        std::cerr << "Warning: submitLimitOrder called with market order\n";
        return -1;
    }

    if (order.price <= 0.0 || order.quantity <= 0.0) {
        std::cerr << "Warning: invalid order (price=" << order.price
                  << ", quantity=" << order.quantity << ")\n";
        return -1;
    }

    // Assign unique ID
    int order_id = next_order_id_++;

    // Store in appropriate queue
    PendingOrder pending(order_id, order, timestamp);

    if (order.side == Side::BUY) {
        buy_orders_.push_back(pending);
    } else {
        sell_orders_.push_back(pending);
    }

    // Check immediately if it crosses (handles "crossing orders")
    checkCrosses(timestamp);

    return order_id;
}

// Cancel a pending order
void ExchangeSimulator::cancelOrder(int order_id) {
    // Remove from buy orders
    auto buy_it = std::remove_if(buy_orders_.begin(), buy_orders_.end(),
        [order_id](const PendingOrder& po) { return po.id == order_id; });

    if (buy_it != buy_orders_.end()) {
        buy_orders_.erase(buy_it, buy_orders_.end());
        return;
    }

    // Remove from sell orders
    auto sell_it = std::remove_if(sell_orders_.begin(), sell_orders_.end(),
        [order_id](const PendingOrder& po) { return po.id == order_id; });

    if (sell_it != sell_orders_.end()) {
        sell_orders_.erase(sell_it, sell_orders_.end());
    }
}

// Retrieve and clear pending fills
std::vector<Fill> ExchangeSimulator::popFills() {
    std::vector<Fill> fills = std::move(pending_fills_);
    pending_fills_.clear();
    return fills;
}

// Check all pending orders for price crosses
void ExchangeSimulator::checkCrosses(uint64_t timestamp) {
    double best_bid = market_.best_bid();
    double best_ask = market_.best_ask();

    // Skip if invalid market state
    if (best_bid <= 0.0 || best_ask <= 0.0) {
        return;
    }

    // Check buy orders (execute if we can buy at/below our limit price)
    for (auto it = buy_orders_.begin(); it != buy_orders_.end(); ) {
        bool should_execute = (best_ask <= it->order.price);

        if (should_execute) {
            auto fills = executeOrder(*it, timestamp);

            // Add fills to pending queue
            pending_fills_.insert(pending_fills_.end(), fills.begin(), fills.end());

            // Track filled quantity
            for (const auto& fill : fills) {
                it->filled_quantity += fill.quantity;
            }

            // Remove if fully filled
            if (it->remaining_quantity() <= 0.0001) {
                it = buy_orders_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    // Check sell orders (execute if we can sell at/above our limit price)
    for (auto it = sell_orders_.begin(); it != sell_orders_.end(); ) {
        bool should_execute = (best_bid >= it->order.price);

        if (should_execute) {
            auto fills = executeOrder(*it, timestamp);

            // Add fills to pending queue
            pending_fills_.insert(pending_fills_.end(), fills.begin(), fills.end());

            // Track filled quantity
            for (const auto& fill : fills) {
                it->filled_quantity += fill.quantity;
            }

            // Remove if fully filled
            if (it->remaining_quantity() <= 0.0001) {
                it = sell_orders_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

// Execute an order by walking the book
std::vector<Fill> ExchangeSimulator::executeOrder(PendingOrder& pending, uint64_t timestamp) {
    std::vector<Fill> fills;

    const Order& order = pending.order;
    double remaining = pending.remaining_quantity();

    // Select appropriate side of book
    const auto& levels = (order.side == Side::BUY) ? market_.asks() : market_.bids();

    // Walk the book up to limit price
    for (const auto& level : levels) {
        if (remaining <= 0.0) break;

        // Price limit check
        if (order.side == Side::BUY && level.price > order.price) {
            break;  // Too expensive for buy order
        }
        if (order.side == Side::SELL && level.price < order.price) {
            break;  // Too cheap for sell order
        }

        // Calculate fill quantity
        double fill_qty = std::min(remaining, level.amount);

        // Create fill
        Fill fill;
        fill.order_id = pending.id;
        fill.side = order.side;
        fill.price = level.price;
        fill.quantity = fill_qty;
        fill.timestamp = timestamp;

        fills.push_back(fill);
        remaining -= fill_qty;
    }

    return fills;
}

} // namespace backtest
