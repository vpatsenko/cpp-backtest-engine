#include "backtest.h"
#include "strategies/stoikov_limit_orders.h"
#include "utils/config.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace backtest;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --config <path>      Path to YAML config file (default: config/stoikov-limit.yaml)\n";
    std::cout << "  --lob <path>         Path to LOB CSV file (overrides config)\n";
    std::cout << "  --trades <path>      Path to trades CSV file (overrides config)\n";
    std::cout << "  --cash <amount>      Initial cash (overrides config)\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --config config/stoikov-limit.yaml\n";
    std::cout << "  " << program_name << " --cash 50000 --lob data/lob.csv\n";
}

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments
        std::string config_file = "config/stoikov-limit.yaml";  // Default config
        std::string lob_path_override;
        std::string trades_path_override;
        double cash_override = 0.0;
        bool has_cash_override = false;

        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
                print_usage(argv[0]);
                return 0;
            } else if (std::strcmp(argv[i], "--config") == 0) {
                if (i + 1 < argc) {
                    config_file = argv[++i];
                } else {
                    std::cerr << "Error: --config requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--lob") == 0) {
                if (i + 1 < argc) {
                    lob_path_override = argv[++i];
                } else {
                    std::cerr << "Error: --lob requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--trades") == 0) {
                if (i + 1 < argc) {
                    trades_path_override = argv[++i];
                } else {
                    std::cerr << "Error: --trades requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--cash") == 0) {
                if (i + 1 < argc) {
                    cash_override = std::stod(argv[++i]);
                    has_cash_override = true;
                } else {
                    std::cerr << "Error: --cash requires an argument\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: Unknown option '" << argv[i] << "'\n";
                print_usage(argv[0]);
                return 1;
            }
        }

        // Load configuration
        std::cout << "Loading configuration from: " << config_file << "\n";
        Config config = Config::load_from_file(config_file);

        // Apply command-line overrides
        if (!lob_path_override.empty()) {
            config.set_lob_path(lob_path_override);
        }
        if (!trades_path_override.empty()) {
            config.set_trades_path(trades_path_override);
        }
        if (has_cash_override) {
            config.set_initial_cash(cash_override);
        }

        const auto& bt_config = config.backtest();
        const auto& strat_config = config.strategy();

        std::cout << "\n=== Backtest Configuration ===\n";
        std::cout << "Initial cash:  $" << bt_config.initial_cash << "\n";
        std::cout << "LOB data:      " << bt_config.lob_path << "\n";
        std::cout << "Trades data:   " << bt_config.trades_path << "\n";
        std::cout << "\n";

        // Load market data
        auto events = DataLoader::load_events(bt_config.lob_path, bt_config.trades_path);

        // Validate strategy configuration (stoikov-limit only)
        if (strat_config.name != "stoikov-limit") {
            std::cerr << "Error: Only 'stoikov-limit' strategy is supported\n";
            std::cerr << "Config has strategy: '" << strat_config.name << "'\n";
            return 1;
        }

        // Check for unknown parameters
        std::vector<std::string> valid_params = {
            "gamma", "kappa", "time_horizon", "order_quantity",
            "max_inventory", "min_spread", "refresh_threshold"
        };
        auto unknown = strat_config.get_unknown_params(valid_params);
        for (const auto& param : unknown) {
            std::cerr << "Warning: Unknown parameter '" << param << "' in strategy config\n";
        }

        // Validate critical parameters
        std::vector<std::string> validation_errors;
        std::string error;

        if (strat_config.has("gamma") && !strat_config.validate_range("gamma", 0.0001, 10.0, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("kappa") && !strat_config.validate_range("kappa", 0.0001, 100.0, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("time_horizon") && !strat_config.validate_range("time_horizon", 1, 86400, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("order_quantity") && !strat_config.validate_range("order_quantity", 1, 1000000, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("max_inventory") && !strat_config.validate_range("max_inventory", 1, 10000000, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("min_spread") && !strat_config.validate_range("min_spread", 0.0, 1.0, &error)) {
            validation_errors.push_back(error);
        }
        if (strat_config.has("refresh_threshold") && !strat_config.validate_range("refresh_threshold", 0.0, 1.0, &error)) {
            validation_errors.push_back(error);
        }

        if (!validation_errors.empty()) {
            std::cerr << "\nConfiguration validation failed:\n";
            for (const auto& err : validation_errors) {
                std::cerr << "  - " << err << "\n";
            }
            return 1;
        }

        // Extract strategy parameters
        double gamma = strat_config.get("gamma", 0.1);
        double kappa = strat_config.get("kappa", 1.5);
        double time_horizon = strat_config.get("time_horizon", 300.0);
        double order_quantity = strat_config.get("order_quantity", 15000.0);
        double max_inventory = strat_config.get("max_inventory", 100000.0);
        double min_spread = strat_config.get("min_spread", 0.00005);
        double refresh_threshold = strat_config.get("refresh_threshold", 0.0001);

        std::cout << "=== Stoikov Limit Order Market Making ===\n";
        std::cout << "Gamma (risk aversion):  " << gamma << "\n";
        std::cout << "Kappa (arrival rate):   " << kappa << "\n";
        std::cout << "Time horizon:           " << time_horizon << " seconds\n";
        std::cout << "Order quantity:         " << order_quantity << " units\n";
        std::cout << "Max inventory:          " << max_inventory << " units\n";
        std::cout << "Min spread:             " << min_spread << "\n";
        std::cout << "Refresh threshold:      " << refresh_threshold << "\n";
        std::cout << "\n";

        // Create strategy
        auto strategy = std::make_shared<StoikovLimitOrderStrategy>(
            gamma, kappa, time_horizon, order_quantity,
            max_inventory, min_spread, refresh_threshold
        );

        // Run backtest
        ReplayEngine engine(strategy, bt_config.initial_cash);
        engine.run(events);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
