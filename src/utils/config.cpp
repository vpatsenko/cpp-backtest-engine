#include "config.h"
#include <fstream>
#include <stdexcept>

namespace backtest {

Config Config::load_from_file(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);

        Config config;

        // Parse backtest section
        if (root["backtest"]) {
            parse_backtest_section(root["backtest"], config.backtest_);
        }

        // Parse strategy section
        if (root["strategy"]) {
            parse_strategy_section(root["strategy"], config.strategy_);
        } else {
            throw std::runtime_error("Missing 'strategy' section in config file");
        }

        return config;

    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to load config file '" + filepath + "': " + e.what());
    }
}

Config Config::parse_yaml(const std::string& yaml_content) {
    try {
        YAML::Node root = YAML::Load(yaml_content);

        Config config;

        if (root["backtest"]) {
            parse_backtest_section(root["backtest"], config.backtest_);
        }

        if (root["strategy"]) {
            parse_strategy_section(root["strategy"], config.strategy_);
        } else {
            throw std::runtime_error("Missing 'strategy' section in config");
        }

        return config;

    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to parse YAML config: " + std::string(e.what()));
    }
}

void Config::parse_backtest_section(const YAML::Node& node, BacktestConfig& config) {
    if (node["initial_cash"]) {
        config.initial_cash = node["initial_cash"].as<double>();
    }

    if (node["lob_path"]) {
        config.lob_path = node["lob_path"].as<std::string>();
    }

    if (node["trades_path"]) {
        config.trades_path = node["trades_path"].as<std::string>();
    }
}

void Config::parse_strategy_section(const YAML::Node& node, StrategyConfig& config) {
    // Strategy name is required
    if (!node["name"]) {
        throw std::runtime_error("Missing 'name' field in strategy section");
    }
    config.name = node["name"].as<std::string>();

    // Parse parameters if present
    if (node["params"]) {
        const YAML::Node& params = node["params"];

        // Iterate through all parameters
        for (auto it = params.begin(); it != params.end(); ++it) {
            std::string key = it->first.as<std::string>();
            double value = it->second.as<double>();
            config.params[key] = value;
        }
    }
}

} // namespace backtest
