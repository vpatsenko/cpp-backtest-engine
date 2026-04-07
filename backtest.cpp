#include "backtest.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace backtest {

// MarketState implementation
void MarketState::apply(const BookUpdateEvent& event) {
    bids_ = event.bids;
    asks_ = event.asks;
}

double MarketState::mid_price() const {
    if (bids_.empty() || asks_.empty()) return 0.0;
    return (best_bid() + best_ask()) / 2.0;
}

double MarketState::microprice() const {
    if (bids_.empty() || asks_.empty()) return mid_price();

    double bid_size = bids_[0].amount;
    double ask_size = asks_[0].amount;
    double bid_price = bids_[0].price;
    double ask_price = asks_[0].price;

    if (bid_size + ask_size == 0.0) return mid_price();

    return (bid_size * ask_price + ask_size * bid_price) / (bid_size + ask_size);
}

// Portfolio implementation
void Portfolio::apply_fill(Side side, double price, double quantity, double fee) {
    int sign = static_cast<int>(side);
    double old_position = position_;

    // Update position
    position_ += sign * quantity;

    // Update cash
    cash_ -= sign * quantity * price + fee;

    // Update realized PnL (FIFO-style)
    if (old_position * position_ < 0.0) {  // Position flip
        double closed_qty = std::min(std::abs(old_position), quantity);
        realized_pnl_ += -sign * closed_qty * (price - avg_entry_price_);
    } else if (std::abs(position_) < std::abs(old_position)) {  // Reducing position
        double closed_qty = std::abs(old_position) - std::abs(position_);
        realized_pnl_ += -sign * closed_qty * (price - avg_entry_price_);
    }

    // Update average entry price
    if (position_ == 0.0) {
        avg_entry_price_ = 0.0;
    } else if (std::abs(position_) > std::abs(old_position)) {  // Increasing position
        double new_qty = std::abs(position_) - std::abs(old_position);
        avg_entry_price_ = (avg_entry_price_ * std::abs(old_position) + price * new_qty)
                          / std::abs(position_);
    }
}

// ExecutionModel implementation
std::vector<Fill> ExecutionModel::execute_market_order(
    const Order& order,
    const MarketState& market,
    uint64_t timestamp
) {
    std::vector<Fill> fills;

    if (!order.is_market()) {
        // For simplicity, limit orders not yet supported
        return fills;
    }

    const auto& levels = (order.side == Side::BUY) ? market.asks() : market.bids();

    double remaining = order.quantity;

    for (const auto& level : levels) {
        if (remaining <= 0.0) break;

        double fill_qty = std::min(remaining, level.amount);

        Fill fill;
        fill.side = order.side;
        fill.price = level.price;
        fill.quantity = fill_qty;
        fill.timestamp = timestamp;

        fills.push_back(fill);
        remaining -= fill_qty;
    }

    return fills;
}

// DataLoader implementation
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

// ReplayEngine implementation
void ReplayEngine::run(const std::vector<std::shared_ptr<Event>>& events) {
    for (const auto& event : events) {
        current_time_ = event->timestamp;

        if (event->type == EventType::BOOK_UPDATE) {
            auto book_event = std::static_pointer_cast<BookUpdateEvent>(event);
            market_.apply(*book_event);
            strategy_->on_book_update(current_time_, market_);

        } else if (event->type == EventType::TRADE) {
            auto trade_event = std::static_pointer_cast<TradeEvent>(event);
            strategy_->on_trade(current_time_, *trade_event);
        }

        // Check if strategy wants to submit an order
        if (strategy_->has_pending_order()) {
            Order order = strategy_->take_pending_order();

            // Execute the order
            auto fills = execution_.execute_market_order(order, market_, current_time_);

            for (const auto& fill : fills) {
                portfolio_.apply_fill(fill.side, fill.price, fill.quantity, 0.0);
                strategy_->on_fill(current_time_, fill);
            }
        }
    }

    // Print final results
    double final_equity = portfolio_.equity(market_.mid_price());
    std::cout << "\n=== Backtest Results ===\n";
    std::cout << "Final Position: " << portfolio_.position() << "\n";
    std::cout << "Final Cash: " << portfolio_.cash() << "\n";
    std::cout << "Final Equity: " << final_equity << "\n";
    std::cout << "Realized PnL: " << portfolio_.realized_pnl() << "\n";
}

} // namespace backtest
