#include "portfolio.h"
#include <cmath>

namespace backtest {

void Portfolio::apply_fill(Side side, double price, double quantity, double fee) {
    int64_t sign = static_cast<int64_t>(side);
    double old_position = position_;

    // Update position
    position_ += quantity * sign;

    // Update cash
    cash_ -= quantity * price * sign + fee;

    // Update realized PnL (FIFO-style)
    if ((old_position * position_) < 0.0) {  // Position flip
        double closed_qty = std::min(std::abs(old_position), quantity);
        realized_pnl_ += closed_qty * (price - avg_entry_price_) * (-sign);
    } else if (std::abs(position_) < std::abs(old_position)) {  // Reducing position
        double closed_qty = std::abs(old_position) - std::abs(position_);
        realized_pnl_ += closed_qty * (price - avg_entry_price_) * (-sign);
    }

    // Update average entry price
    if (position_ == 0.0) {
        avg_entry_price_ = 0.0;
    } else if (std::abs(position_) > std::abs(old_position)) {  // Increasing position
        double new_qty = std::abs(position_) - std::abs(old_position);
        avg_entry_price_ = (avg_entry_price_ * std::abs(old_position) + price * new_qty)
                          / std::abs(position_);
    }
}

} // namespace backtest
