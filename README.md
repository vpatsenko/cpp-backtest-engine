# HFT Backtesting Framework

Implementation of the Avellaneda-Stoikov market making model for high-frequency trading.

**Paper**: Avellaneda & Stoikov (2008) - ["High-frequency trading in a limit order book"](https://www.math.nyu.edu/faculty/avellane/HighFrequencyTrading.pdf)

## Strategy Overview

The **Avellaneda-Stoikov Market Making** strategy is a mathematically optimal approach to market making that balances profit maximization with inventory risk management.

### Core Model

The strategy uses three formulas to determine optimal bid and ask quotes:

**1. Reservation Price** - Fair value adjusted for inventory:
```
r = s - q·γ·σ²·(T-t)
```

**2. Optimal Spread** - Distance from reservation price:
```
δ = γ·σ²·(T-t) + (2/γ)·ln(1 + γ/κ)
```

**3. Quotes** - Placed symmetrically:
```
bid = r - δ/2
ask = r + δ/2
```

**Where:**
- `s` = mid-price
- `q` = inventory (+ = long, - = short)
- `γ` = risk aversion (higher = wider spreads)
- `κ` = arrival rate (higher = tighter spreads)
- `σ` = volatility
- `T-t` = time remaining

### How It Works

**When inventory = 0:**
Quotes are centered around mid-price, earning spread on both sides.

**When long (q > 0):**
Reservation price drops → quotes shift lower → eager to sell

**When short (q < 0):**
Reservation price rises → quotes shift higher → eager to buy

**As time → horizon:**
Spreads narrow → quotes converge to mid-price

### Key Parameters

| Parameter | Default | Effect |
|-----------|---------|--------|
| **gamma (γ)** | 0.1 | Risk aversion (0.05=aggressive, 0.2=conservative) |
| **kappa (κ)** | 1.5 | Liquidity assumption (0.5=low, 3.0=high) |
| **time_horizon** | 300s | Trading period duration |

**See [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for detailed parameter tuning.**

## Build Instructions

### Prerequisites

**1. Install Conan:**
```bash
# macOS
brew install conan

# Linux
pip3 install conan

# First time setup
conan profile detect --force
```

**2. Download Data Files:**

Download from: https://drive.google.com/file/d/1DiP5arvCEMxLHZ0R2mAS4lcMjnPHSrEJ/edit

Extract and place in project root:
- `lob.csv` (~925 MB)
- `trades.csv` (~946 MB)

### Build

```bash
# Install dependencies
conan install . --output-folder=build --build=missing

# Configure
cmake --preset conan-release

# Compile
cmake --build build/build/Release
```

**Build time:** ~30 seconds

### Rebuild (after code changes)

```bash
cmake --build build/build/Release
```

## Running the Backtest

### Basic Usage

```bash
# Run with default config
./build/build/Release/backtest

# Use custom config
./build/build/Release/backtest --config config/stoikov-limit.yaml
```

### Command Line Options

```bash
# Override parameters
./build/build/Release/backtest --cash 50000
./build/build/Release/backtest --lob custom-lob.csv --trades custom-trades.csv

# View all options
./build/build/Release/backtest --help
```

### Expected Output

```
=== Backtest Configuration ===
Initial cash:  $100000
LOB data:      lob.csv
Trades data:   trades.csv

Loaded 22901679 events

=== Stoikov Limit Order Market Making ===
Gamma (risk aversion):  0.1
Kappa (arrival rate):   1.5
Time horizon:           300 seconds

Posted BID #1: 15000 @ 0.0109216
Posted ASK #2: 15000 @ 0.0111422
...

=== PERFORMANCE METRICS ===
Total Return:        -0.12%
Sharpe Ratio:        -11.63
Win Rate:            52.63%
Total Trades:        76
```

## Configuration

Edit `config/stoikov-limit.yaml`:

```yaml
backtest:
  initial_cash: 100000.0
  lob_path: "lob.csv"
  trades_path: "trades.csv"

strategy:
  name: "stoikov-limit"
  params:
    gamma: 0.1              # Risk aversion
    kappa: 1.5              # Arrival rate
    time_horizon: 300.0     # Time horizon (seconds)
    order_quantity: 15000.0
    max_inventory: 100000.0
    min_spread: 0.00005
    refresh_threshold: 0.0001
```

**Configuration presets:**

**Conservative:** `gamma: 0.2, kappa: 1.0` → Wide spreads, low risk  
**Balanced (default):** `gamma: 0.1, kappa: 1.5` → Moderate  
**Aggressive:** `gamma: 0.05, kappa: 3.0` → Tight spreads, high activity

## Troubleshooting

**"conan: command not found"**
```bash
brew install conan  # or: pip3 install conan
```

**"Cannot find boost or yaml-cpp"**
```bash
rm -rf build
conan install . --output-folder=build --build=missing
```

**"lob.csv: No such file or directory"**
```bash
# Download data files and place in project root
ls -lh *.csv  # Should show lob.csv (925MB), trades.csv (946MB)
```

**Build errors after git pull**
```bash
rm -rf build
conan install . --output-folder=build --build=missing
cmake --preset conan-release
cmake --build build/build/Release
```

## Documentation

- **[AVELLANEDA_STOIKOV_IMPLEMENTATION.md](AVELLANEDA_STOIKOV_IMPLEMENTATION.md)** - Mathematical formulas and implementation details
- **[CONFIG_GUIDE.md](CONFIG_GUIDE.md)** - Complete parameter tuning guide with examples
- **Original Paper** - Avellaneda & Stoikov (2008) in `HighFrequencyTradingInALimitOrderbook.pdf`

## Quick Reference

| Task | Command |
|------|---------|
| **Build** | `conan install . --output-folder=build --build=missing && cmake --preset conan-release && cmake --build build/build/Release` |
| **Rebuild** | `cmake --build build/build/Release` |
| **Run** | `./build/build/Release/backtest` |
| **Clean** | `rm -rf build` |
