#include "replay_engine.h"
#include "../execution/exchange_simulator.h"
#include "../utils/performance_tracker.h"
#include <iostream>
#include <memory>

namespace backtest {

ReplayEngine::ReplayEngine(std::shared_ptr<IStrategy> strategy, double initial_cash)
    : strategy_(strategy), portfolio_(initial_cash), exchange_(std::make_unique<ExchangeSimulator>()) {}

ReplayEngine::~ReplayEngine() = default;

void ReplayEngine::run(const std::vector<std::shared_ptr<Event>>& events) {
    // Initialize performance tracker
    double initial_equity = portfolio_.equity(0.0);  // Initial equity (no position yet)
    perf_tracker_ = std::make_unique<PerformanceTracker>(initial_equity);

    // Record initial snapshot
    if (!events.empty()) {
        current_time_ = events[0]->timestamp;
        perf_tracker_->record_snapshot(current_time_, initial_equity,
                                       portfolio_.cash(), portfolio_.position(), 0.0);
    }

    size_t book_update_count = 0;  // For periodic snapshots

    for (const auto& event : events) {
        current_time_ = event->timestamp;

        if (event->type == EventType::BOOK_UPDATE) {
            auto book_event = std::static_pointer_cast<BookUpdateEvent>(event);

            // Update exchange (triggers cross checks for limit orders)
            exchange_->onBookUpdate(*book_event, current_time_);

            // Process any fills generated from limit order crosses
            if (exchange_->hasPendingFills()) {
                auto fills = exchange_->popFills();
                for (const auto& fill : fills) {
                    double prev_realized_pnl = portfolio_.realized_pnl();
                    portfolio_.apply_fill(fill.side, fill.price, fill.quantity, 0.0);
                    double realized_pnl_delta = portfolio_.realized_pnl() - prev_realized_pnl;

                    strategy_->on_fill(current_time_, fill);

                    // Record fill and snapshot
                    perf_tracker_->record_fill(current_time_, fill, realized_pnl_delta);
                    double mark = market_.mid_price();
                    perf_tracker_->record_snapshot(current_time_, portfolio_.equity(mark),
                                                   portfolio_.cash(), portfolio_.position(), mark);
                }
            }

            // Update convenience market state and notify strategy
            market_.apply(*book_event);

            // Periodic snapshots (every 1000 book updates)
            if (++book_update_count % 1000 == 0) {
                double mark = market_.mid_price();
                perf_tracker_->record_snapshot(current_time_, portfolio_.equity(mark),
                                               portfolio_.cash(), portfolio_.position(), mark);
            }

            strategy_->on_book_update(current_time_, market_);

        } else if (event->type == EventType::TRADE) {
            auto trade_event = std::static_pointer_cast<TradeEvent>(event);
            strategy_->on_trade(current_time_, *trade_event);
        }

        // Check if strategy wants to cancel orders
        if (strategy_->has_pending_cancellations()) {
            auto cancellations = strategy_->take_pending_cancellations();
            for (int order_id : cancellations) {
                exchange_->cancelOrder(order_id);
            }
        }

        // Check if strategy wants to submit orders
        while (strategy_->has_pending_orders()) {
            auto orders = strategy_->take_pending_orders();

            for (const auto& order : orders) {
                if (order.is_market()) {
                    // Market orders: execute immediately via old ExecutionModel
                    auto fills = execution_.execute_market_order(order, market_, current_time_);
                    for (const auto& fill : fills) {
                        double prev_realized_pnl = portfolio_.realized_pnl();
                        portfolio_.apply_fill(fill.side, fill.price, fill.quantity, 0.0);
                        double realized_pnl_delta = portfolio_.realized_pnl() - prev_realized_pnl;

                        strategy_->on_fill(current_time_, fill);

                        // Record fill and snapshot
                        perf_tracker_->record_fill(current_time_, fill, realized_pnl_delta);
                        double mark = market_.mid_price();
                        perf_tracker_->record_snapshot(current_time_, portfolio_.equity(mark),
                                                       portfolio_.cash(), portfolio_.position(), mark);
                    }
                } else {
                    // Limit orders: submit to exchange simulator
                    int order_id = exchange_->submitLimitOrder(order, current_time_);

                    // Notify strategy of order ID
                    strategy_->on_order_submitted(order_id, order);

                    // Process any immediate fills (handles crossing orders)
                    if (exchange_->hasPendingFills()) {
                        auto fills = exchange_->popFills();
                        for (const auto& fill : fills) {
                            double prev_realized_pnl = portfolio_.realized_pnl();
                            portfolio_.apply_fill(fill.side, fill.price, fill.quantity, 0.0);
                            double realized_pnl_delta = portfolio_.realized_pnl() - prev_realized_pnl;

                            strategy_->on_fill(current_time_, fill);

                            // Record fill and snapshot
                            perf_tracker_->record_fill(current_time_, fill, realized_pnl_delta);
                            double mark = market_.mid_price();
                            perf_tracker_->record_snapshot(current_time_, portfolio_.equity(mark),
                                                           portfolio_.cash(), portfolio_.position(), mark);
                        }
                    }
                }
            }
        }
    }

    // Record final snapshot
    double final_mark = market_.mid_price();
    double final_equity = portfolio_.equity(final_mark);
    perf_tracker_->record_snapshot(current_time_, final_equity,
                                   portfolio_.cash(), portfolio_.position(), final_mark);

    // Print performance report
    std::cout << "\n";
    perf_tracker_->print_detailed_report();

    // Print portfolio state
    std::cout << "\n=== Portfolio State ===\n";
    std::cout << "Final Position: " << portfolio_.position() << "\n";
    std::cout << "Final Cash: $" << portfolio_.cash() << "\n";
    std::cout << "Final Equity: $" << final_equity << "\n";
    std::cout << "Pending Limit Orders: " << exchange_->pendingOrderCount() << "\n";
}

} // namespace backtest
