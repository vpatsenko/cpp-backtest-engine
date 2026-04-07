#pragma once

#include "backtest.h"
#include <deque>
#include <iostream>

namespace backtest {

// Buy and hold strategy - simple baseline
class BuyHoldStrategy : public IStrategy {
public:
    BuyHoldStrategy(double quantity) : quantity_(quantity), has_bought_(false) {}

    void on_book_update(uint64_t timestamp, const MarketState& market) override {
        if (!has_bought_ && market.best_ask() > 0.0) {
            Order order;
            order.side = Side::BUY;
            order.quantity = quantity_;
            order.price = 0.0;  // Market order
            submit_order(order);
            has_bought_ = true;
        }
    }

    void on_trade(uint64_t, const TradeEvent&) override {}
    void on_fill(uint64_t timestamp, const Fill& fill) override {
        std::cout << "Fill at " << timestamp << ": "
                  << (fill.side == Side::BUY ? "BUY" : "SELL")
                  << " " << fill.quantity << " @ " << fill.price << "\n";
    }

private:
    double quantity_;
    bool has_bought_;
};

// Mean reversion strategy
class MeanReversionStrategy : public IStrategy {
public:
    MeanReversionStrategy(size_t window_size = 100,
                         double threshold = 0.0005,
                         double trade_size = 10000.0)
        : window_size_(window_size)
        , threshold_(threshold)
        , trade_size_(trade_size)
        , position_(0.0) {}

    void on_book_update(uint64_t, const MarketState& market) override {
        double mid = market.mid_price();
        if (mid <= 0.0) return;

        // Update price window
        price_window_.push_back(mid);
        if (price_window_.size() > window_size_) {
            price_window_.pop_front();
        }

        if (price_window_.size() < window_size_) return;

        // Calculate mean and standard deviation
        double sum = 0.0;
        for (double p : price_window_) {
            sum += p;
        }
        double mean = sum / price_window_.size();

        // Mean reversion: buy when price below mean, sell when above
        double deviation = (mid - mean) / mean;

        if (deviation < -threshold_ && position_ <= 0.0 && !has_pending_order()) {
            // Price below mean - buy
            Order order;
            order.side = Side::BUY;
            order.quantity = trade_size_;
            order.price = 0.0;
            submit_order(order);
        } else if (deviation > threshold_ && position_ >= 0.0 && !has_pending_order()) {
            // Price above mean - sell
            Order order;
            order.side = Side::SELL;
            order.quantity = trade_size_;
            order.price = 0.0;
            submit_order(order);
        }
    }

    void on_trade(uint64_t, const TradeEvent&) override {}

    void on_fill(uint64_t, const Fill& fill) override {
        int sign = static_cast<int>(fill.side);
        position_ += sign * fill.quantity;
    }

private:
    size_t window_size_;
    double threshold_;
    double trade_size_;
    double position_;
    std::deque<double> price_window_;
};

// Order book imbalance strategy
class ImbalanceStrategy : public IStrategy {
public:
    ImbalanceStrategy(double threshold = 0.3, double trade_size = 5000.0)
        : threshold_(threshold)
        , trade_size_(trade_size)
        , tick_count_(0) {}

    void on_book_update(uint64_t, const MarketState& market) override {
        tick_count_++;

        // Trade only every N ticks to reduce churn
        if (tick_count_ % 100 != 0) return;

        const auto& bids = market.bids();
        const auto& asks = market.asks();

        if (bids.empty() || asks.empty()) return;

        // Calculate top-of-book imbalance
        double bid_size = bids[0].amount;
        double ask_size = asks[0].amount;
        double total = bid_size + ask_size;

        if (total <= 0.0) return;

        double imbalance = (bid_size - ask_size) / total;

        // Strong bid imbalance -> price likely going up -> buy
        if (imbalance > threshold_ && !has_pending_order()) {
            Order order;
            order.side = Side::BUY;
            order.quantity = trade_size_;
            order.price = 0.0;
            submit_order(order);
        }
        // Strong ask imbalance -> price likely going down -> sell
        else if (imbalance < -threshold_ && !has_pending_order()) {
            Order order;
            order.side = Side::SELL;
            order.quantity = trade_size_;
            order.price = 0.0;
            submit_order(order);
        }
    }

    void on_trade(uint64_t, const TradeEvent&) override {}
    void on_fill(uint64_t, const Fill&) override {}

private:
    double threshold_;
    double trade_size_;
    size_t tick_count_;
};

} // namespace backtest
