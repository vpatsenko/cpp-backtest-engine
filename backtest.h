#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <map>

namespace backtest {

enum class EventType {
    BOOK_UPDATE,
    TRADE,
    ORDER_FILL
};

enum class Side {
    BUY = 1,
    SELL = -1
};

struct Event {
    EventType type;
    uint64_t timestamp;

    virtual ~Event() = default;
};

struct BookLevel {
    double price;
    double amount;

    BookLevel(double p = 0.0, double a = 0.0) : price(p), amount(a) {}
};

struct BookUpdateEvent : Event {
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;

    BookUpdateEvent() {
        type = EventType::BOOK_UPDATE;
    }
};

struct TradeEvent : Event {
    Side side;
    double price;
    double amount;

    TradeEvent() {
        type = EventType::TRADE;
    }
};

class MarketState {
public:
    void apply(const BookUpdateEvent& event);

    double best_bid() const { return bids_.empty() ? 0.0 : bids_[0].price; }
    double best_ask() const { return asks_.empty() ? 0.0 : asks_[0].price; }
    double mid_price() const;
    double microprice() const;

    const std::vector<BookLevel>& bids() const { return bids_; }
    const std::vector<BookLevel>& asks() const { return asks_; }

private:
    std::vector<BookLevel> bids_;
    std::vector<BookLevel> asks_;
};

class Portfolio {
public:
    Portfolio(double initial_cash = 0.0)
        : cash_(initial_cash), position_(0.0) {}

    void apply_fill(Side side, double price, double quantity, double fee = 0.0);

    double equity(double mark_price) const {
        return cash_ + position_ * mark_price;
    }

    double cash() const { return cash_; }
    double position() const { return position_; }
    double realized_pnl() const { return realized_pnl_; }

private:
    double cash_;
    double position_;
    double realized_pnl_ = 0.0;
    double avg_entry_price_ = 0.0;
};

struct Order {
    Side side;
    double quantity;
    double price;  // 0.0 for market order

    bool is_market() const { return price == 0.0; }
};

struct Fill {
    Side side;
    double price;
    double quantity;
    uint64_t timestamp;
};

class IStrategy {
public:
    virtual ~IStrategy() = default;

    virtual void on_book_update(uint64_t timestamp, const MarketState& market) = 0;
    virtual void on_trade(uint64_t timestamp, const TradeEvent& trade) = 0;
    virtual void on_fill(uint64_t timestamp, const Fill& fill) = 0;

    // Strategy can emit orders
    bool has_pending_order() const { return pending_order_.has_value(); }
    Order take_pending_order() {
        Order o = *pending_order_;
        pending_order_.reset();
        return o;
    }

protected:
    void submit_order(const Order& order) {
        pending_order_ = order;
    }

private:
    std::optional<Order> pending_order_;
};

// Simple execution model - market orders walk the book
class ExecutionModel {
public:
    std::vector<Fill> execute_market_order(
        const Order& order,
        const MarketState& market,
        uint64_t timestamp
    );
};

// Data loader
class DataLoader {
public:
    static std::vector<std::shared_ptr<Event>> load_events(
        const std::string& lob_path,
        const std::string& trades_path
    );
};

// Replay engine - coordinates the backtest
class ReplayEngine {
public:
    ReplayEngine(std::shared_ptr<IStrategy> strategy, double initial_cash = 100000.0)
        : strategy_(strategy), portfolio_(initial_cash) {}

    void run(const std::vector<std::shared_ptr<Event>>& events);

    const Portfolio& portfolio() const { return portfolio_; }
    const MarketState& market_state() const { return market_; }

private:
    std::shared_ptr<IStrategy> strategy_;
    MarketState market_;
    Portfolio portfolio_;
    ExecutionModel execution_;
    uint64_t current_time_ = 0;
};

} // namespace backtest
