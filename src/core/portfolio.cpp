#include "portfolio.h"
#include <cmath>
#include <algorithm>

namespace backtest {

void Portfolio::apply_fill(Side side, double price, double quantity, double fee) {
    int sign = static_cast<int>(side);
    double old_position = position_;

    // Update position
    position_ += sign * quantity;

    // Update cash
    cash_ -= sign * quantity * price + fee;

    // Update realized PnL (FIFO-style)
    if (old_position * position_ < 0.0) {  // Position flip
        double closed_qty = std::min(std::abs(old_position), quantity);
        realized_pnl_ += -sign * closed_qty * (price - avg_entry_price_);
    } else if (std::abs(position_) < std::abs(old_position)) {  // Reducing position
        double closed_qty = std::abs(old_position) - std::abs(position_);
        realized_pnl_ += -sign * closed_qty * (price - avg_entry_price_);
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
