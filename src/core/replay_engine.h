#pragma once

#include "types.h"
#include "market_state.h"
#include "portfolio.h"
#include "strategy.h"
#include "../execution/execution_model.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace backtest {

// Forward declarations
class ExchangeSimulator;
class PerformanceTracker;

// Replay engine - coordinates the backtest
class ReplayEngine {
public:
    ReplayEngine(std::shared_ptr<IStrategy> strategy, double initial_cash = 100000.0);
    ~ReplayEngine();

    void run(const std::vector<std::shared_ptr<Event>>& events);

    const Portfolio& portfolio() const { return portfolio_; }
    const MarketState& market_state() const { return market_; }
    const PerformanceTracker* performance_tracker() const { return perf_tracker_.get(); }

private:
    std::shared_ptr<IStrategy> strategy_;
    MarketState market_;
    Portfolio portfolio_;
    ExecutionModel execution_;  // Keep for backward compatibility with market orders
    std::unique_ptr<ExchangeSimulator> exchange_;
    std::unique_ptr<PerformanceTracker> perf_tracker_;
    uint64_t current_time_ = 0;
};

} // namespace backtest
