#pragma once

#include "types.h"
#include <vector>

namespace backtest {

class MarketState {
public:
    void apply(const BookUpdateEvent& event);

    double best_bid() const { return bids_.empty() ? 0.0 : bids_[0].price; }
    double best_ask() const { return asks_.empty() ? 0.0 : asks_[0].price; }
    double mid_price() const;
    double microprice() const;

    const std::vector<BookLevel>& bids() const { return bids_; }
    const std::vector<BookLevel>& asks() const { return asks_; }

private:
    std::vector<BookLevel> bids_;
    std::vector<BookLevel> asks_;
};

} // namespace backtest
