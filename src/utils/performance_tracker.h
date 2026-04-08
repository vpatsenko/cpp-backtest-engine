#pragma once

#include "../core/types.h"
#include <vector>
#include <cmath>
#include <numeric>

namespace backtest {

// Snapshot of equity at a point in time
struct EquitySnapshot {
    uint64_t timestamp;
    double equity;
    double cash;
    double position;
    double mark_price;
};

// Record of a trade execution
struct TradeRecord {
    uint64_t timestamp;
    Side side;
    double price;
    double quantity;
    double realized_pnl_delta;  // Change in realized PnL from this trade
};

// Comprehensive performance metrics
struct PerformanceMetrics {
    // Returns & Risk
    double total_return;
    double sharpe_ratio;
    double max_drawdown;
    double max_drawdown_duration_ms;
    double volatility;

    // Trading Activity
    int num_trades;
    int num_winning_trades;
    int num_losing_trades;
    double win_rate;

    // Trade Statistics
    double avg_profit_per_trade;
    double avg_win;
    double avg_loss;
    double largest_win;
    double largest_loss;
    double profit_factor;  // Gross profits / Gross losses

    // Time Period
    uint64_t start_time;
    uint64_t end_time;
    double duration_days;
};

class PerformanceTracker {
public:
    explicit PerformanceTracker(double initial_equity);

    // Data collection
    void record_snapshot(uint64_t timestamp, double equity, double cash,
                        double position, double mark_price);
    void record_fill(uint64_t timestamp, const Fill& fill, double realized_pnl_delta);

    // Metrics calculation
    PerformanceMetrics calculate_metrics() const;

    // Reporting
    void print_report() const;
    void print_detailed_report() const;

    // Access to raw data
    const std::vector<EquitySnapshot>& snapshots() const { return equity_curve_; }
    const std::vector<TradeRecord>& trades() const { return trade_log_; }

private:
    double initial_equity_;
    std::vector<EquitySnapshot> equity_curve_;
    std::vector<TradeRecord> trade_log_;

    // Helper methods for metrics calculation
    double calculate_sharpe_ratio() const;
    double calculate_max_drawdown() const;
    std::pair<double, uint64_t> calculate_max_drawdown_and_duration() const;
    double calculate_volatility() const;
    double calculate_win_rate() const;
    void calculate_trade_statistics(double& avg_win, double& avg_loss,
                                    double& largest_win, double& largest_loss,
                                    double& profit_factor, int& num_wins, int& num_losses) const;
};

} // namespace backtest
