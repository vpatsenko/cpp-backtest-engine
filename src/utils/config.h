#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace backtest {

// Configuration for backtest execution
struct BacktestConfig {
    double initial_cash = 100000.0;
    std::string lob_path = "lob.csv";
    std::string trades_path = "trades.csv";
};

// Configuration for strategy parameters
// Note: Currently only supports double type for parameters
// For boolean/string types, consider extending to std::variant or YAML::Node
struct StrategyConfig {
    std::string name;
    std::unordered_map<std::string, double> params;

    // Helper to get parameter with default value
    double get(const std::string& key, double default_value = 0.0) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : default_value;
    }

    // Helper to check if parameter exists
    bool has(const std::string& key) const {
        return params.find(key) != params.end();
    }

    // Get list of parameters that are in config but not in expected list
    std::vector<std::string> get_unknown_params(const std::vector<std::string>& valid_keys) const {
        std::vector<std::string> unknown;
        for (const auto& [key, _] : params) {
            if (std::find(valid_keys.begin(), valid_keys.end(), key) == valid_keys.end()) {
                unknown.push_back(key);
            }
        }
        return unknown;
    }

    // Check if all required parameters are present
    std::vector<std::string> get_missing_params(const std::vector<std::string>& required_keys) const {
        std::vector<std::string> missing;
        for (const auto& key : required_keys) {
            if (!has(key)) {
                missing.push_back(key);
            }
        }
        return missing;
    }

    // Validate parameter value is in valid range
    bool validate_range(const std::string& key, double min_val, double max_val, std::string* error_msg = nullptr) const {
        if (!has(key)) {
            if (error_msg) {
                *error_msg = "Parameter '" + key + "' not found";
            }
            return false;
        }
        double val = get(key);
        if (val < min_val || val > max_val) {
            if (error_msg) {
                *error_msg = "Parameter '" + key + "' value " + std::to_string(val) +
                           " is out of valid range [" + std::to_string(min_val) + ", " +
                           std::to_string(max_val) + "]";
            }
            return false;
        }
        return true;
    }
};

// Main configuration class
class Config {
public:
    // Default constructor
    Config() = default;

    // Load configuration from YAML file
    static Config load_from_file(const std::string& filepath);

    // Parse configuration from YAML string (for testing)
    static Config parse_yaml(const std::string& yaml_content);

    // Accessors
    const BacktestConfig& backtest() const { return backtest_; }
    const StrategyConfig& strategy() const { return strategy_; }

    // Mutators (for command-line overrides)
    void set_lob_path(const std::string& path) { backtest_.lob_path = path; }
    void set_trades_path(const std::string& path) { backtest_.trades_path = path; }
    void set_initial_cash(double cash) { backtest_.initial_cash = cash; }
    void set_strategy_name(const std::string& name) { strategy_.name = name; }

private:
    BacktestConfig backtest_;
    StrategyConfig strategy_;

    // Internal parsing methods
    static void parse_backtest_section(const YAML::Node& node, BacktestConfig& config);
    static void parse_strategy_section(const YAML::Node& node, StrategyConfig& config);
};

} // namespace backtest
