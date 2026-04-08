#include "execution_model.h"
#include <algorithm>

namespace backtest {

std::vector<Fill> ExecutionModel::execute_market_order(
    const Order& order,
    const MarketState& market,
    uint64_t timestamp
) {
    std::vector<Fill> fills;

    if (!order.is_market()) {
        // For simplicity, limit orders not yet supported
        return fills;
    }

    const auto& levels = (order.side == Side::BUY) ? market.asks() : market.bids();

    double remaining = order.quantity;

    for (const auto& level : levels) {
        if (remaining <= 0.0) break;

        double fill_qty = std::min(remaining, level.amount);

        Fill fill;
        fill.order_id = 0;  // Market orders have no ID (executed immediately)
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
