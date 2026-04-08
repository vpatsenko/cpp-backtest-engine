#pragma once

#include "../core/types.h"
#include "../core/market_state.h"
#include <vector>
#include <cstdint>

namespace backtest {

// Simple execution model - market orders walk the book
class ExecutionModel {
public:
    std::vector<Fill> execute_market_order(
        const Order& order,
        const MarketState& market,
        uint64_t timestamp
    );
};

} // namespace backtest
