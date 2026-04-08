#include "performance_tracker.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace backtest {

PerformanceTracker::PerformanceTracker(double initial_equity)
    : initial_equity_(initial_equity) {}

void PerformanceTracker::record_snapshot(uint64_t timestamp, double equity, double cash,
                                         double position, double mark_price) {
    EquitySnapshot snapshot;
    snapshot.timestamp = timestamp;
    snapshot.equity = equity;
    snapshot.cash = cash;
    snapshot.position = position;
    snapshot.mark_price = mark_price;
    equity_curve_.push_back(snapshot);
}

void PerformanceTracker::record_fill(uint64_t timestamp, const Fill& fill, double realized_pnl_delta) {
    TradeRecord trade;
    trade.timestamp = timestamp;
    trade.side = fill.side;
    trade.price = fill.price;
    trade.quantity = fill.quantity;
    trade.realized_pnl_delta = realized_pnl_delta;
    trade_log_.push_back(trade);
}

double PerformanceTracker::calculate_sharpe_ratio() const {
    if (equity_curve_.size() < 2) return 0.0;

    // Calculate returns between snapshots
    std::vector<double> returns;
    for (size_t i = 1; i < equity_curve_.size(); ++i) {
        double ret = (equity_curve_[i].equity - equity_curve_[i-1].equity)
                     / equity_curve_[i-1].equity;
        returns.push_back(ret);
    }

    if (returns.empty()) return 0.0;

    // Mean return
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    // Standard deviation
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    double std_dev = std::sqrt(variance / returns.size());

    if (std_dev == 0.0) return 0.0;

    // Annualize (assuming snapshots represent equal time intervals)
    uint64_t duration_us = equity_curve_.back().timestamp - equity_curve_.front().timestamp;
    double duration_years = duration_us / (365.25 * 24 * 3600 * 1e6);

    if (duration_years <= 0.0) return 0.0;

    double periods_per_year = returns.size() / duration_years;

    double sharpe = (mean_return * periods_per_year) / (std_dev * std::sqrt(periods_per_year));
    return sharpe;
}

double PerformanceTracker::calculate_max_drawdown() const {
    auto [dd, duration] = calculate_max_drawdown_and_duration();
    return dd;
}

std::pair<double, uint64_t> PerformanceTracker::calculate_max_drawdown_and_duration() const {
    if (equity_curve_.empty()) return {0.0, 0};

    double peak = equity_curve_[0].equity;
    double max_dd = 0.0;
    uint64_t max_dd_duration = 0;
    uint64_t dd_start_time = 0;
    bool in_drawdown = false;

    for (const auto& snapshot : equity_curve_) {
        if (snapshot.equity > peak) {
            peak = snapshot.equity;
            in_drawdown = false;
        } else {
            double drawdown = (peak - snapshot.equity) / peak;
            if (drawdown > max_dd) {
                max_dd = drawdown;
            }

            if (!in_drawdown) {
                dd_start_time = snapshot.timestamp;
                in_drawdown = true;
            }

            uint64_t dd_duration = snapshot.timestamp - dd_start_time;
            if (dd_duration > max_dd_duration) {
                max_dd_duration = dd_duration;
            }
        }
    }

    return {max_dd, max_dd_duration};
}

double PerformanceTracker::calculate_volatility() const {
    if (equity_curve_.size() < 2) return 0.0;

    // Calculate returns
    std::vector<double> returns;
    for (size_t i = 1; i < equity_curve_.size(); ++i) {
        double ret = (equity_curve_[i].equity - equity_curve_[i-1].equity)
                     / equity_curve_[i-1].equity;
        returns.push_back(ret);
    }

    if (returns.empty()) return 0.0;

    // Mean return
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    // Standard deviation
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    double std_dev = std::sqrt(variance / returns.size());

    // Annualize
    uint64_t duration_us = equity_curve_.back().timestamp - equity_curve_.front().timestamp;
    double duration_years = duration_us / (365.25 * 24 * 3600 * 1e6);

    if (duration_years <= 0.0) return 0.0;

    double periods_per_year = returns.size() / duration_years;

    return std_dev * std::sqrt(periods_per_year);
}

double PerformanceTracker::calculate_win_rate() const {
    if (trade_log_.empty()) return 0.0;

    int winning_trades = 0;
    int total_trades = 0;

    for (const auto& trade : trade_log_) {
        if (trade.realized_pnl_delta != 0.0) {
            total_trades++;
            if (trade.realized_pnl_delta > 0.0) {
                winning_trades++;
            }
        }
    }

    if (total_trades == 0) return 0.0;

    return static_cast<double>(winning_trades) / total_trades;
}

void PerformanceTracker::calculate_trade_statistics(double& avg_win, double& avg_loss,
                                                     double& largest_win, double& largest_loss,
                                                     double& profit_factor, int& num_wins, int& num_losses) const {
    double total_wins = 0.0;
    double total_losses = 0.0;
    num_wins = 0;
    num_losses = 0;
    largest_win = 0.0;
    largest_loss = 0.0;

    for (const auto& trade : trade_log_) {
        if (trade.realized_pnl_delta > 0.0) {
            total_wins += trade.realized_pnl_delta;
            num_wins++;
            if (trade.realized_pnl_delta > largest_win) {
                largest_win = trade.realized_pnl_delta;
            }
        } else if (trade.realized_pnl_delta < 0.0) {
            total_losses += trade.realized_pnl_delta;
            num_losses++;
            if (trade.realized_pnl_delta < largest_loss) {
                largest_loss = trade.realized_pnl_delta;
            }
        }
    }

    avg_win = (num_wins > 0) ? total_wins / num_wins : 0.0;
    avg_loss = (num_losses > 0) ? total_losses / num_losses : 0.0;
    profit_factor = (total_losses != 0.0) ? total_wins / std::abs(total_losses) : 0.0;
}

PerformanceMetrics PerformanceTracker::calculate_metrics() const {
    PerformanceMetrics metrics = {};

    if (equity_curve_.empty()) return metrics;

    // Returns & Risk
    double final_equity = equity_curve_.back().equity;
    metrics.total_return = (final_equity - initial_equity_) / initial_equity_;
    metrics.sharpe_ratio = calculate_sharpe_ratio();
    auto [max_dd, dd_duration] = calculate_max_drawdown_and_duration();
    metrics.max_drawdown = max_dd;
    metrics.max_drawdown_duration_ms = dd_duration / 1000.0;  // Convert microseconds to milliseconds
    metrics.volatility = calculate_volatility();

    // Trading Activity
    int num_winning = 0;
    int num_losing = 0;
    for (const auto& trade : trade_log_) {
        if (trade.realized_pnl_delta != 0.0) {
            if (trade.realized_pnl_delta > 0.0) num_winning++;
            else num_losing++;
        }
    }
    metrics.num_trades = num_winning + num_losing;
    metrics.num_winning_trades = num_winning;
    metrics.num_losing_trades = num_losing;
    metrics.win_rate = calculate_win_rate();

    // Trade Statistics
    double avg_win, avg_loss, largest_win, largest_loss, profit_factor;
    int num_wins, num_losses;
    calculate_trade_statistics(avg_win, avg_loss, largest_win, largest_loss,
                              profit_factor, num_wins, num_losses);

    metrics.avg_profit_per_trade = (metrics.num_trades > 0) ?
        (final_equity - initial_equity_) / metrics.num_trades : 0.0;
    metrics.avg_win = avg_win;
    metrics.avg_loss = avg_loss;
    metrics.largest_win = largest_win;
    metrics.largest_loss = largest_loss;
    metrics.profit_factor = profit_factor;

    // Time Period
    metrics.start_time = equity_curve_.front().timestamp;
    metrics.end_time = equity_curve_.back().timestamp;
    metrics.duration_days = (metrics.end_time - metrics.start_time) / (24.0 * 3600 * 1e6);

    return metrics;
}

void PerformanceTracker::print_report() const {
    auto metrics = calculate_metrics();

    std::cout << "=== PERFORMANCE METRICS ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total Return:        " << (metrics.total_return * 100) << "%\n";
    std::cout << "Sharpe Ratio:        " << metrics.sharpe_ratio << "\n";
    std::cout << "Max Drawdown:        " << (metrics.max_drawdown * 100) << "%\n";
    std::cout << "Volatility:          " << (metrics.volatility * 100) << "%\n";
    std::cout << "\n";
    std::cout << "Total Trades:        " << metrics.num_trades << "\n";
    std::cout << "Win Rate:            " << (metrics.win_rate * 100) << "%\n";
    std::cout << "Avg Profit/Trade:    $" << metrics.avg_profit_per_trade << "\n";
    std::cout << "\n";
    std::cout << "Duration:            " << metrics.duration_days << " days\n";
}

void PerformanceTracker::print_detailed_report() const {
    auto metrics = calculate_metrics();

    std::cout << "=== PERFORMANCE METRICS ===\n\n";

    std::cout << "Returns & Risk:\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total Return:        " << (metrics.total_return * 100) << "%\n";
    std::cout << "  Sharpe Ratio:        " << metrics.sharpe_ratio << "\n";
    std::cout << "  Max Drawdown:        " << (metrics.max_drawdown * 100) << "%\n";
    std::cout << "  Max DD Duration:     " << (metrics.max_drawdown_duration_ms / 1000.0) << " seconds\n";
    std::cout << "  Volatility:          " << (metrics.volatility * 100) << "% (annualized)\n";
    std::cout << "\n";

    std::cout << "Trading Activity:\n";
    std::cout << "  Total Trades:        " << metrics.num_trades << "\n";
    std::cout << "  Winning Trades:      " << metrics.num_winning_trades
              << " (" << std::setprecision(2) << (metrics.win_rate * 100) << "%)\n";
    std::cout << "  Losing Trades:       " << metrics.num_losing_trades
              << " (" << std::setprecision(2) << ((1.0 - metrics.win_rate) * 100) << "%)\n";
    std::cout << "\n";

    std::cout << "  Avg Profit/Trade:    $" << std::setprecision(2) << metrics.avg_profit_per_trade << "\n";
    std::cout << "  Avg Win:             $" << metrics.avg_win << "\n";
    std::cout << "  Avg Loss:            $" << metrics.avg_loss << "\n";
    std::cout << "  Largest Win:         $" << metrics.largest_win << "\n";
    std::cout << "  Largest Loss:        $" << metrics.largest_loss << "\n";
    std::cout << "  Profit Factor:       " << std::setprecision(2) << metrics.profit_factor << "\n";
    std::cout << "\n";

    std::cout << "Time Period:\n";
    // Convert timestamps to human-readable format
    time_t start_sec = metrics.start_time / 1000000;
    time_t end_sec = metrics.end_time / 1000000;
    std::cout << "  Start:               " << std::ctime(&start_sec);
    std::cout << "  End:                 " << std::ctime(&end_sec);
    std::cout << "  Duration:            " << std::setprecision(2) << metrics.duration_days << " days\n";
}

} // namespace backtest
