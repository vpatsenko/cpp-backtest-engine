cmake_minimum_required(VERSION 3.14)
project(cmf_backtest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan integration (if building with Conan, the toolchain is auto-included)
# For manual builds, the toolchain file is passed via -DCMAKE_TOOLCHAIN_FILE

# Find dependencies (provided by Conan or system)
find_package(Boost REQUIRED)
find_package(yaml-cpp REQUIRED)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Common sources from src/
set(COMMON_SOURCES
    src/core/market_state.cpp
    src/core/portfolio.cpp
    src/core/replay_engine.cpp
    src/execution/execution_model.cpp
    src/execution/exchange_simulator.cpp
    src/data/data_loader.cpp
    src/utils/performance_tracker.cpp
    src/utils/config.cpp
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Main backtest executable with multiple strategy support
add_executable(backtest
    main.cpp
    ${COMMON_SOURCES}
)

target_link_libraries(backtest PRIVATE yaml-cpp::yaml-cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(backtest PRIVATE -Wall -Wextra -pedantic)
elseif(MSVC)
    target_compile_options(backtest PRIVATE /W4)
endif()
