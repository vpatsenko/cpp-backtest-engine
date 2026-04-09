#include "market_state.h"

namespace backtest {

void MarketState::apply(const BookUpdateEvent& event) {
    bids_ = event.bids;
    asks_ = event.asks;
}

double MarketState::mid_price() const {
    if (bids_.empty() || asks_.empty()) return 0.0;
    return (best_bid() + best_ask()) / 2.0;
}

double MarketState::microprice() const {
    if (bids_.empty() || asks_.empty()) return mid_price();

    double bid_size = bids_[0].amount;
    double ask_size = asks_[0].amount;
    double bid_price = bids_[0].price;
    double ask_price = asks_[0].price;

    if ((bid_size + ask_size) == 0.0) return mid_price();

    return (bid_size * ask_price + ask_size * bid_price) / (bid_size + ask_size);
}

} // namespace backtest
