#include "backtest.h"
#include <iostream>
#include <deque>

using namespace backtest;

// Simple momentum strategy: buy when microprice > moving average, sell when below
class SimpleStrategy : public IStrategy {
public:
    SimpleStrategy(size_t window_size = 100, double trade_size = 10000.0)
        : window_size_(window_size), trade_size_(trade_size) {}

    void on_book_update(uint64_t timestamp, const MarketState& market) override {
        double microprice = market.microprice();
        if (microprice <= 0.0) return;

        // Update moving average
        price_window_.push_back(microprice);
        if (price_window_.size() > window_size_) {
            price_window_.pop_front();
        }

        if (price_window_.size() < window_size_) return;  // Wait for window to fill

        // Calculate moving average
        double sum = 0.0;
        for (double p : price_window_) {
            sum += p;
        }
        double ma = sum / price_window_.size();

        // Simple signal: current price vs MA
        if (microprice > ma * 1.0001 && !has_pending_order()) {  // Small threshold
            // Buy signal
            Order order;
            order.side = Side::BUY;
            order.quantity = trade_size_;
            order.price = 0.0;  // Market order
            submit_order(order);

        } else if (microprice < ma * 0.9999 && !has_pending_order()) {
            // Sell signal
            Order order;
            order.side = Side::SELL;
            order.quantity = trade_size_;
            order.price = 0.0;  // Market order
            submit_order(order);
        }
    }

    void on_trade(uint64_t timestamp, const TradeEvent& trade) override {
        // Not used in this simple strategy
    }

    void on_fill(uint64_t timestamp, const Fill& fill) override {
        std::cout << "Fill at " << timestamp << ": "
                  << (fill.side == Side::BUY ? "BUY" : "SELL")
                  << " " << fill.quantity << " @ " << fill.price << "\n";
    }

private:
    size_t window_size_;
    double trade_size_;
    std::deque<double> price_window_;
};

int main(int argc, char* argv[]) {
    try {
        std::string lob_path = "lob.csv";
        std::string trades_path = "trades.csv";

        if (argc > 2) {
            lob_path = argv[1];
            trades_path = argv[2];
        }

        std::cout << "Loading data...\n";
        auto events = DataLoader::load_events(lob_path, trades_path);

        std::cout << "Running backtest with simple momentum strategy...\n";
        auto strategy = std::make_shared<SimpleStrategy>(100, 10000.0);
        ReplayEngine engine(strategy, 100000.0);

        engine.run(events);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
