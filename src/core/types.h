#pragma once

#include <cstdint>
#include <vector>

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

struct Order {
    Side side;
    double quantity;
    double price;  // 0.0 for market order

    bool is_market() const { return price == 0.0; }
};

struct Fill {
    int order_id;       // Links fill to originating order
    Side side;
    double price;
    double quantity;
    uint64_t timestamp;
};

} // namespace backtest
