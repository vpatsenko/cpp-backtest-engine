# HFT Backtesting Framework

Implementation of the Avellaneda-Stoikov market making model for high-frequency trading.

**Paper**: "High-frequency trading in a limit order book" (Quantitative Finance, 2008)

## Quick Start

```bash
# 1. Install Conan
brew install conan && conan profile detect --force

# 2. Download data files (see below)

# 3. Build
conan install . --output-folder=build --build=missing
cmake --preset conan-release
cmake --build build/build/Release

# 4. Run
./build/build/Release/backtest
```

**→ See [Getting Started](#getting-started) section below for detailed instructions.**

## Features

### Backtesting Engine
- ✅ Historical market data replay (LOB + trades)
- ✅ Time-controlled event processing
- ✅ PnL tracking and performance metrics
- ✅ Configurable via YAML files

### Exchange Simulator
- ✅ Limit order execution with price-cross logic
- ✅ Realistic book walking for VWAP fills
- ✅ Partial fill handling
- ✅ FIFO order processing

### Stoikov Market Making Strategy
- ✅ Avellaneda-Stoikov optimal quoting model
- ✅ Inventory risk management
- ✅ Dynamic spread adjustment
- ✅ Time-decaying spreads
- ✅ Configurable risk parameters

## Strategy: Avellaneda-Stoikov Market Making

### Overview

The **Stoikov Limit Order Market Making** strategy implements the Avellaneda-Stoikov optimal quoting model from their seminal 2008 paper ["High-frequency trading in a limit order book"](https://www.math.nyu.edu/faculty/avellane/HighFrequencyTrading.pdf). This is a mathematically rigorous approach to market making that balances:

- **Profit maximization** - Capturing bid-ask spread
- **Inventory risk management** - Avoiding large positions
- **Market impact** - Calibrating to order flow dynamics

### How It Works

The strategy operates in three steps:

**1. Calculate Reservation Price**

The reservation price represents the fair value adjusted for current inventory:

```
r(s, q, t) = s - q·γ·σ²·(T-t)
```

- `s` = current mid-price
- `q` = inventory (positive = long, negative = short)
- `γ` = risk aversion parameter
- `σ` = volatility (per second)
- `(T-t)` = time remaining

**Effect**: When long inventory (q > 0), reservation price drops below mid-price, making the strategy **eager to sell**. When short inventory (q < 0), reservation price rises above mid-price, making the strategy **eager to buy**.

**2. Calculate Optimal Spread**

The spread determines how far from the reservation price to place quotes:

```
δ* = γ·σ²·(T-t) + (2/γ)·ln(1 + γ/κ)
     ─────────────   ─────────────────
     Inventory risk   Liquidity component
```

- **Inventory risk term**: Wider spread when more time remaining (more uncertain)
- **Liquidity term**: Calibrates to expected order arrival rate κ

**Effect**: Spread widens with volatility, risk aversion, and time remaining. Spread narrows as the strategy approaches its time horizon.

**3. Place Quotes Symmetrically**

```
bid = r - δ/2
ask = r + δ/2
```

Quotes are placed symmetrically around the reservation price, ensuring the strategy leans in the direction needed to reduce inventory.

### Strategy Behavior

**Normal Operation (q ≈ 0):**
```
Market:  [==========|==========]  mid = $100.00
Strategy:      bid ← r → ask
               $99.89 ← $100.00 → $100.11
```
Quotes centered around mid-price, earning spread on both sides.

**Long Inventory (q > 0):**
```
Market:  [==========|==========]  mid = $100.00
Strategy: bid ← r    → ask
          $99.84 ← $99.95 → $100.06
```
Reservation price shifts lower, quotes skew lower to incentivize selling.

**Short Inventory (q < 0):**
```
Market:  [==========|==========]  mid = $100.00
Strategy:        bid ← r → ask
                 $99.94 ← $100.05 → $100.16
```
Reservation price shifts higher, quotes skew higher to incentivize buying.

**Near Time Horizon (T-t → 0):**
```
Market:  [==========|==========]  mid = $100.00
Strategy:       bid ← r → ask
                $99.98 ← $100.00 → $100.02
```
Spread narrows as uncertainty decreases, quotes converge to mid-price.

### Parameter Guide

| Parameter | Symbol | Effect | Typical Range |
|-----------|--------|--------|---------------|
| **Risk Aversion** | γ | Higher → Wider spreads, more conservative | 0.01 - 1.0 |
| **Arrival Rate** | κ | Higher → Tighter spreads, assumes more liquidity | 0.5 - 5.0 |
| **Time Horizon** | T | Trading period duration | 60s - 600s |
| **Volatility** | σ | Estimated from market data | Auto-computed |

**Risk Aversion (γ):**
- `γ = 0.05`: Aggressive (tight spreads, higher inventory risk)
- `γ = 0.10`: Moderate (balanced approach) ← **Default**
- `γ = 0.20`: Conservative (wide spreads, minimal inventory risk)

**Arrival Rate (κ):**
- `κ = 0.5`: Low liquidity assumption (wide spreads)
- `κ = 1.5`: Moderate liquidity ← **Default**
- `κ = 3.0`: High liquidity assumption (tight spreads)

### Strategy Advantages

✅ **Theoretically Optimal**: Derived from utility maximization  
✅ **Inventory-Aware**: Automatically manages position risk  
✅ **Adaptive**: Adjusts to volatility and time remaining  
✅ **Well-Studied**: Widely used in academic and industry research  
✅ **Parameter Intuitive**: Clear economic interpretation  

### Strategy Limitations

⚠️ **Assumes Brownian Mid-Price**: Market may have trends or jumps  
⚠️ **No Adverse Selection**: Doesn't account for informed traders  
⚠️ **Symmetric Information**: Assumes fair playing field  
⚠️ **Continuous Trading**: Discrete updates can cause slippage  
⚠️ **Static Parameters**: γ and κ don't adapt to regime changes  

### Performance Characteristics

**Typical Performance (6-day backtest):**
```
Total Return:        -0.12% to +0.15%
Sharpe Ratio:        -11 to -4 (high frequency, small returns)
Win Rate:            50-55%
Total Trades:        50-250 depending on parameters
Max Drawdown:        0.1-0.2%
```

**What Affects Performance:**
- **Volatility**: Higher σ → wider spreads → fewer trades → lower returns
- **Order Flow**: Balanced buy/sell flow → better performance
- **Spreads**: Tighter spreads (low γ) → more trades → higher PnL variance
- **Inventory Management**: Better γ/κ balance → lower inventory risk

### Example Configurations

**Conservative (Low Risk):**
```yaml
strategy:
  params:
    gamma: 0.2              # High risk aversion
    kappa: 1.0              # Low arrival rate assumption
    time_horizon: 600.0     # Long time horizon
    min_spread: 0.0001      # Wide minimum spread
```

**Balanced (Default):**
```yaml
strategy:
  params:
    gamma: 0.1              # Moderate risk aversion
    kappa: 1.5              # Moderate arrival rate
    time_horizon: 300.0     # 5-minute horizon
    min_spread: 0.00005     # 5 tick minimum
```

**Aggressive (High Activity):**
```yaml
strategy:
  params:
    gamma: 0.05             # Low risk aversion
    kappa: 3.0              # High arrival rate assumption
    time_horizon: 120.0     # Short 2-minute horizon
    min_spread: 0.00001     # Tight minimum spread
```

### Further Reading

**Academic Papers:**
- Avellaneda & Stoikov (2008) - Original paper
- Guéant, Lehalle & Fernandez-Tapia (2013) - Extended model with asymmetric information
- Cartea & Jaimungal (2015) - Algorithmic and High-Frequency Trading textbook

**Implementation Details:**
- [AVELLANEDA_STOIKOV_IMPLEMENTATION.md](AVELLANEDA_STOIKOV_IMPLEMENTATION.md) - Mathematical formulas and code mapping
- [CONFIG_GUIDE.md](CONFIG_GUIDE.md) - Parameter tuning guide with examples

**Research Applications:**
- Market microstructure analysis
- High-frequency trading strategy development
- Optimal execution research
- Inventory risk management studies

## Getting Started

### Step 1: Install Build Tools

**On macOS:**
```bash
# Install Conan package manager
brew install conan

# Detect system configuration (first time only)
conan profile detect --force
```

**On Linux:**
```bash
# Install Conan via pip
pip3 install conan

# Detect system configuration (first time only)
conan profile detect --force

# Install system dependencies
sudo apt-get install cmake build-essential  # Debian/Ubuntu
# OR
sudo yum install cmake gcc-c++              # RedHat/CentOS
```

**Alternative: Use System Package Manager**
```bash
# macOS
brew install boost yaml-cpp cmake

# Linux (Debian/Ubuntu)
sudo apt-get install libboost-dev libyaml-cpp-dev cmake
```

### Step 2: Download Data Files

Download simulation data from: https://drive.google.com/file/d/1DiP5arvCEMxLHZ0R2mAS4lcMjnPHSrEJ/edit

Extract and place in project root:
```
cmf/
├── lob.csv        # ~925 MB - Limit order book snapshots
├── trades.csv     # ~946 MB - Trade events
└── ...
```

### Step 3: Build the Project

**Method A: Using Conan (Recommended)**

```bash
# Navigate to project directory
cd /path/to/cmf

# Install dependencies (boost, yaml-cpp)
conan install . --output-folder=build --build=missing

# Configure build system
cmake --preset conan-release

# Compile (takes ~30 seconds)
cmake --build build/build/Release

# Verify build
ls -lh build/build/Release/backtest
# Output: -rwxr-xr-x ... 323K ... backtest
```

**Method B: Using System Libraries**

```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build

# Verify
ls -lh build/backtest
```

### Step 4: Run Your First Backtest

```bash
# Using Conan build
./build/build/Release/backtest

# Using system library build
./build/backtest
```

**Expected Output:**
```
Loading configuration from: config/stoikov-limit.yaml

=== Backtest Configuration ===
Initial cash:  $100000
LOB data:      lob.csv
Trades data:   trades.csv

Loaded 22901679 events
=== Stoikov Limit Order Market Making ===
Gamma (risk aversion):  0.1
Kappa (arrival rate):   1.5
Time horizon:           300 seconds
...

Posted BID #1: 15000 @ 0.0109216
Posted ASK #2: 15000 @ 0.0111422
...

=== PERFORMANCE METRICS ===
Total Return:        -0.12%
Sharpe Ratio:        -11.63
...
```

## Build Instructions Reference

### Quick Commands

| Task | Command |
|------|---------|
| **Clean build** | `rm -rf build && conan install . --output-folder=build --build=missing` |
| **Configure** | `cmake --preset conan-release` |
| **Compile** | `cmake --build build/build/Release` |
| **Rebuild** | `cmake --build build/build/Release` (after code changes) |
| **Run** | `./build/build/Release/backtest` |
| **Help** | `./build/build/Release/backtest --help` |

### Usage Examples

**Default run:**
```bash
./build/build/Release/backtest
# Uses config/stoikov-limit.yaml automatically
```

**Custom configuration:**
```bash
./build/build/Release/backtest --config config/my-strategy.yaml
```

**Override parameters:**
```bash
# Change initial capital
./build/build/Release/backtest --cash 50000

# Use different data files
./build/build/Release/backtest --lob data/custom-lob.csv --trades data/custom-trades.csv

# Combine multiple options
./build/build/Release/backtest --config config/stoikov-limit.yaml --cash 200000
```

**View all options:**
```bash
./build/build/Release/backtest --help
```

### Development Workflow

**After modifying C++ code:**
```bash
# Just recompile (fast)
cmake --build build/build/Release

# Run
./build/build/Release/backtest
```

**After modifying configuration:**
```bash
# No rebuild needed, just run
./build/build/Release/backtest --config config/stoikov-limit.yaml
```

**After changing dependencies:**
```bash
# Clean and rebuild everything
rm -rf build
conan install . --output-folder=build --build=missing
cmake --preset conan-release
cmake --build build/build/Release
```

### Troubleshooting

**Problem: "conan: command not found"**
```bash
# Install Conan
brew install conan          # macOS
# OR
pip3 install conan          # Linux/macOS
```

**Problem: "Cannot find boost" or "Cannot find yaml-cpp"**
```bash
# Reinstall dependencies
rm -rf build
conan install . --output-folder=build --build=missing
```

**Problem: "lob.csv: No such file or directory"**
```bash
# Download data files (see Step 2 above)
# Place lob.csv and trades.csv in project root directory
ls -lh *.csv
# Should show: lob.csv (925MB), trades.csv (946MB)
```

**Problem: Build errors after git pull**
```bash
# Clean rebuild
rm -rf build
conan install . --output-folder=build --build=missing
cmake --preset conan-release
cmake --build build/build/Release
```

**Problem: Slow compilation**
```bash
# Use parallel compilation
cmake --build build/build/Release -j$(nproc)    # Linux
cmake --build build/build/Release -j$(sysctl -n hw.ncpu)  # macOS
```

### Platform-Specific Notes

**macOS (Apple Silicon):**
- Build produces ARM64 executable
- Native performance, no Rosetta needed
- Conan automatically detects architecture

**macOS (Intel):**
- Build produces x86_64 executable
- Works identically to ARM64

**Linux:**
- Requires GCC 7+ or Clang 5+
- Supports both x86_64 and ARM64
- May need to install poppler-utils for PDF reading

**Windows:**
- Use WSL2 (Windows Subsystem for Linux) recommended
- Or use Visual Studio with vcpkg instead of Conan
- See CONFIG_GUIDE.md for Windows-specific instructions

See [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for parameter tuning and configuration options.

## Performance Metrics

The framework automatically tracks and reports:

**Returns & Risk:**
- Total Return
- Sharpe Ratio
- Maximum Drawdown
- Volatility (annualized)

**Trading Activity:**
- Total Trades
- Win Rate
- Winning/Losing Trade Counts

**Trade Statistics:**
- Average Profit per Trade
- Average Win/Loss
- Largest Win/Loss
- Profit Factor

## Project Structure

```
├── src/
│   ├── core/           # Backtesting engine core
│   │   ├── market_state.{h,cpp}     # Market state representation
│   │   ├── portfolio.{h,cpp}         # Portfolio management
│   │   ├── replay_engine.{h,cpp}    # Event replay engine
│   │   └── strategy.h                # Strategy interface
│   ├── execution/      # Exchange simulation
│   │   ├── exchange_simulator.{h,cpp}  # Order matching engine
│   │   └── execution_model.{h,cpp}     # Execution models
│   ├── data/           # Data loading
│   │   └── data_loader.{h,cpp}       # CSV data loader
│   ├── strategies/     # Trading strategies
│   │   └── stoikov_limit_orders.{h,cpp}  # Stoikov strategy
│   └── utils/          # Utilities
│       ├── config.{h,cpp}            # YAML configuration
│       └── performance_tracker.{h,cpp}  # Performance metrics
├── config/             # Strategy configurations
│   └── stoikov-limit.yaml
├── main.cpp            # Entry point
└── CONFIG_GUIDE.md     # Configuration documentation
```
