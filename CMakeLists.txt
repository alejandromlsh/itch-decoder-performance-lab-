cmake_minimum_required(VERSION 3.18)
project(nyse_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / IDE support

# ── DPDK via pkg-config ─────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(nyse_decoder
    src/main.cpp
    src/dpdk/eal_init.cpp
    src/dpdk/port_manager.cpp
    # src/decode/scalar/scalar_decoder.cpp
    src/io/binary_dump.hpp
    # src/io/stats.cpp
)

target_include_directories(nyse_decoder PRIVATE
    src
    ${DPDK_INCLUDE_DIRS}
)

target_compile_options(nyse_decoder PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    ${DPDK_CFLAGS_OTHER}
)

target_link_libraries(nyse_decoder PRIVATE
    ${DPDK_LIBRARIES}
    pthread dl
)

# Per-file optimisation flags — each decoder TU gets its own flags
set_source_files_properties(src/decode/scalar/scalar_decoder.cpp
    PROPERTIES COMPILE_OPTIONS "-O2")