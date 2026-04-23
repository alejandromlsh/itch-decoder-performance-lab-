cmake_minimum_required(VERSION 3.18)
project(nyse_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / IDE support

# Default to Release build for performance 
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# DPDK via pkg-config 
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Main executable 
add_executable(nyse_decoder
    src/main.cpp
    src/dpdk/eal_init.cpp
    src/dpdk/port_manager.cpp
    # src/decode/scalar/scalar_decoder.cpp
    src/io/binary_dump.hpp
    src/order_book/order_book.hpp
    src/perf/perf_stats.hpp
    # src/io/stats.cpp
)

target_include_directories(nyse_decoder PRIVATE
    src
    ${DPDK_INCLUDE_DIRS}
)

# Performance and Warning Flags
target_compile_options(nyse_decoder PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    
    # Apply these flags ONLY in Release mode
    $<$<CONFIG:Release>:
        -O3             # Maximum optimization
        -march=native   # Use CPU-specific instructions
    >
    ${DPDK_CFLAGS_OTHER}
)
# Link-Time Optimization (LTO) - Allows inlining across .cpp files
# set_property(TARGET nyse_decoder PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_link_libraries(nyse_decoder PRIVATE
    ${DPDK_LIBRARIES}
    pthread dl
)
