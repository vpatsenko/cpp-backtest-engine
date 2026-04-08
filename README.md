# CMF Assignment

Simulation data: https://drive.google.com/file/d/1DiP5arvCEMxLHZ0R2mAS4lcMjnPHSrEJ/edit

## Exchange Simulator Implementation

This project now includes a **complete Exchange Simulator** that implements limit order execution with price-cross logic.

### Features
- ✅ Limit order support with price-cross execution
- ✅ Realistic book walking for VWAP fills
- ✅ Partial fill handling
- ✅ Market order backward compatibility
- ✅ FIFO order processing

### Limit Order Execution
Limit orders execute when market price crosses the order level:
- **Buy limit**: Executes when `best_ask ≤ limit_price`
- **Sell limit**: Executes when `best_bid ≥ limit_price`

### Usage Example

```cpp
// Market order (executes immediately)
Order market_order;
market_order.side = Side::BUY;
market_order.quantity = 10000.0;
market_order.price = 0.0;  // price = 0 means market order
submit_order(market_order);

// Limit order (executes on price cross)
Order limit_order;
limit_order.side = Side::BUY;
limit_order.quantity = 10000.0;
limit_order.price = 0.0110;  // specific price = limit order
submit_order(limit_order);
```

### Building
```bash
mkdir build && cd build
cmake ..
make
```

### Testing
```bash
# Run main backtest (market orders)
./backtest lob.csv trades.csv

# Run limit order test
./test_limit_orders lob.csv trades.csv

# Run partial fill test
./test_partial_fills lob.csv trades.csv
```

### Architecture
- `exchange_simulator.h/cpp` - Exchange matching engine
- `backtest.h/cpp` - Backtesting framework
- `main.cpp` - Market order strategy example
- `test_limit_orders.cpp` - Limit order test
- `test_partial_fills.cpp` - Partial fill test
