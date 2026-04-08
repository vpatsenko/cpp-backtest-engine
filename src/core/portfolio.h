#pragma once

#include "types.h"

namespace backtest {

class Portfolio {
public:
    Portfolio(double initial_cash = 0.0)
        : cash_(initial_cash), position_(0.0) {}

    void apply_fill(Side side, double price, double quantity, double fee = 0.0);

    double equity(double mark_price) const {
        return cash_ + position_ * mark_price;
    }

    double cash() const { return cash_; }
    double position() const { return position_; }
    double realized_pnl() const { return realized_pnl_; }

private:
    double cash_;
    double position_;
    double realized_pnl_ = 0.0;
    double avg_entry_price_ = 0.0;
};

} // namespace backtest
