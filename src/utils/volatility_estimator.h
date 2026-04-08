#pragma once

#include <deque>
#include <cmath>

namespace backtest {

// Volatility estimator using rolling window of log returns
class VolatilityEstimator {
public:
    explicit VolatilityEstimator(size_t window_size = 1000)
        : window_size_(window_size), last_price_(0.0) {}

    void update(double price) {
        if (last_price_ > 0.0 && price > 0.0) {
            double log_return = std::log(price / last_price_);
            returns_.push_back(log_return);

            if (returns_.size() > window_size_) {
                returns_.pop_front();
            }
        }
        last_price_ = price;
    }

    double get_volatility() const {
        if (returns_.size() < 2) return 0.01;  // Default 1% volatility

        // Calculate standard deviation
        double sum = 0.0;
        for (double ret : returns_) {
            sum += ret;
        }
        double mean = sum / returns_.size();

        double variance = 0.0;
        for (double ret : returns_) {
            variance += (ret - mean) * (ret - mean);
        }
        double std_dev = std::sqrt(variance / returns_.size());

        // Annualize: assuming each observation is ~25 seconds (1000 ticks * 25 sec)
        // There are ~1.26M seconds in a year
        double periods_per_year = 1260000.0 / 25.0;  // ~50400
        double annualized_vol = std_dev * std::sqrt(periods_per_year);

        return annualized_vol;
    }

private:
    size_t window_size_;
    double last_price_;
    std::deque<double> returns_;
};

} // namespace backtest
