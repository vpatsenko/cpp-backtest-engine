#include "backtest.h"
#include "strategies/stoikov_limit_orders.h"
#include "strategies/stoikov_market_orders.h"
#include <iostream>
#include <deque>
#include <string>
#include <cstring>

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
        if (microprice > ma * 1.0001 && !has_pending_orders()) {  // Small threshold
            // Buy signal
            Order order;
            order.side = Side::BUY;
            order.quantity = trade_size_;
            order.price = 0.0;  // Market order
            submit_order(order);

        } else if (microprice < ma * 0.9999 && !has_pending_orders()) {
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

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --strategy <name>    Strategy to run (default: stoikov-limit)\n";
    std::cout << "                       Options: simple, stoikov-limit, stoikov-market\n";
    std::cout << "  --lob <path>         Path to LOB CSV file (default: lob.csv)\n";
    std::cout << "  --trades <path>      Path to trades CSV file (default: trades.csv)\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --strategy stoikov-limit\n";
    std::cout << "  " << program_name << " --strategy simple --lob data/lob.csv --trades data/trades.csv\n";
}

int main(int argc, char* argv[]) {
    try {
        std::string strategy_name = "stoikov-limit";
        std::string lob_path = "lob.csv";
        std::string trades_path = "trades.csv";

        // Parse command-line arguments
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
                print_usage(argv[0]);
                return 0;
            } else if (std::strcmp(argv[i], "--strategy") == 0) {
                if (i + 1 < argc) {
                    strategy_name = argv[++i];
                } else {
                    std::cerr << "Error: --strategy requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--lob") == 0) {
                if (i + 1 < argc) {
                    lob_path = argv[++i];
                } else {
                    std::cerr << "Error: --lob requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--trades") == 0) {
                if (i + 1 < argc) {
                    trades_path = argv[++i];
                } else {
                    std::cerr << "Error: --trades requires an argument\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: Unknown option '" << argv[i] << "'\n";
                print_usage(argv[0]);
                return 1;
            }
        }

        std::cout << "Loading data from:\n";
        std::cout << "  LOB: " << lob_path << "\n";
        std::cout << "  Trades: " << trades_path << "\n";
        auto events = DataLoader::load_events(lob_path, trades_path);

        // Create strategy based on user selection
        std::shared_ptr<IStrategy> strategy;

        if (strategy_name == "simple") {
            std::cout << "\nRunning backtest with Simple Momentum Strategy\n";
            std::cout << "Strategy Parameters:\n";
            std::cout << "  Window size:         100\n";
            std::cout << "  Trade size:          10,000 units\n";
            std::cout << "\n";
            strategy = std::make_shared<SimpleStrategy>(100, 10000.0);

        } else if (strategy_name == "stoikov-limit") {
            std::cout << "\nRunning backtest with Stoikov Limit Order Market Making\n";
            std::cout << "Strategy Parameters:\n";
            std::cout << "  Gamma (risk aversion):   0.05\n";
            std::cout << "  Kappa (arrival rate):    2.0\n";
            std::cout << "  Time horizon:            300 seconds\n";
            std::cout << "  Order quantity:          15,000 units\n";
            std::cout << "  Max inventory:           100,000 units\n";
            std::cout << "  Min spread:              0.00001 (1 tick)\n";
            std::cout << "  Refresh threshold:       0.0002 (2bp)\n";
            std::cout << "\n";
            strategy = std::make_shared<StoikovLimitOrderStrategy>(
                0.05,      // gamma - risk aversion
                2.0,       // kappa - arrival rate
                300.0,     // time_horizon - 5 minutes
                15000.0,   // order_quantity
                100000.0,  // max_inventory
                0.00001,   // min_spread - 1 tick
                0.0002     // refresh_threshold - 2bp move
            );

        } else if (strategy_name == "stoikov-market") {
            std::cout << "\nRunning backtest with Stoikov Market Order Strategy\n";
            std::cout << "Strategy Parameters:\n";
            std::cout << "  Gamma (risk aversion):   0.05\n";
            std::cout << "  Kappa (arrival rate):    2.0\n";
            std::cout << "  Time horizon:            300 seconds\n";
            std::cout << "  Order quantity:          15,000 units\n";
            std::cout << "  Max inventory:           100,000 units\n";
            std::cout << "  Min spread:              0.00001 (1 tick)\n";
            std::cout << "  Refresh threshold:       0.0002 (2bp)\n";
            std::cout << "\n";
            strategy = std::make_shared<StoikovMarketMakingStrategy>(
                0.05,      // gamma - risk aversion
                2.0,       // kappa - arrival rate
                300.0,     // time_horizon - 5 minutes
                15000.0,   // order_quantity
                100000.0,  // max_inventory
                0.00001,   // min_spread - 1 tick
                0.0002     // refresh_threshold - 2bp move
            );

        } else {
            std::cerr << "Error: Unknown strategy '" << strategy_name << "'\n";
            std::cerr << "Available strategies: simple, stoikov-limit, stoikov-market\n";
            return 1;
        }

        ReplayEngine engine(strategy, 100000.0);  // $100K initial capital
        engine.run(events);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
