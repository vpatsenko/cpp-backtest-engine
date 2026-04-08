#include "data_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace backtest {

std::vector<std::shared_ptr<Event>> DataLoader::load_events(
    const std::string& lob_path,
    const std::string& trades_path
) {
    std::vector<std::shared_ptr<Event>> events;

    // Load LOB events
    std::ifstream lob_file(lob_path);
    if (!lob_file.is_open()) {
        throw std::runtime_error("Failed to open " + lob_path);
    }

    std::string line;
    std::getline(lob_file, line);  // Skip header

    while (std::getline(lob_file, line)) {
        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 102) continue;  // 1 index + 1 timestamp + 25*2*2 = 102

        auto event = std::make_shared<BookUpdateEvent>();
        event->timestamp = std::stoull(tokens[1]);

        // Parse asks and bids (25 levels each)
        for (int i = 0; i < 25; ++i) {
            double ask_price = std::stod(tokens[2 + i * 2]);
            double ask_amount = std::stod(tokens[3 + i * 2]);
            event->asks.emplace_back(ask_price, ask_amount);

            double bid_price = std::stod(tokens[52 + i * 2]);
            double bid_amount = std::stod(tokens[53 + i * 2]);
            event->bids.emplace_back(bid_price, bid_amount);
        }

        events.push_back(event);
    }
    lob_file.close();

    // Load trade events
    std::ifstream trades_file(trades_path);
    if (!trades_file.is_open()) {
        throw std::runtime_error("Failed to open " + trades_path);
    }

    std::getline(trades_file, line);  // Skip header

    while (std::getline(trades_file, line)) {
        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 5) continue;

        auto event = std::make_shared<TradeEvent>();
        event->timestamp = std::stoull(tokens[1]);
        event->side = (tokens[2] == "buy") ? Side::BUY : Side::SELL;
        event->price = std::stod(tokens[3]);
        event->amount = std::stod(tokens[4]);

        events.push_back(event);
    }
    trades_file.close();

    // Sort all events by timestamp
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
        return a->timestamp < b->timestamp;
    });

    std::cout << "Loaded " << events.size() << " events\n";
    return events;
}

} // namespace backtest
