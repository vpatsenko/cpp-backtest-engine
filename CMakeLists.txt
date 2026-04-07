cmake_minimum_required(VERSION 3.14)
project(cmf_backtest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Sources
add_executable(backtest
    main.cpp
    backtest.cpp
)

# Headers
target_include_directories(backtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(backtest PRIVATE -Wall -Wextra -pedantic)
elseif(MSVC)
    target_compile_options(backtest PRIVATE /W4)
endif()
