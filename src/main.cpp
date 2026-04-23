#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <rte_eal.h>

#include "dpdk/eal_init.hpp"
#include "dpdk/port_manager.hpp"
#include "dpdk/rx_loop.hpp"

#include "decoder/scalar_decoder.hpp"
#include "order_book/order_book.hpp"
#include "io/binary_dump.hpp"

static constexpr const char* DUMP_PATH = "/home/alejandro/workspace/1-nasdaq-parser/data/nyse_dump2.bin";

// Signal handling

// g_stop is written by the SIGINT/SIGTERM handler and read inside run_rx_loop.

static volatile bool g_stop = false;
static void on_signal(int) { g_stop = true; }

// ─────────────────────────────────────────────────────────────────────────────
// main
//
// Launch command:
// I have isolated core 4 in grub to have minimum interruptions, so use such core
//
// sudo ./nyse_decoder --no-huge -l 4 --vdev 
//      "net_pcap0,rx_pcap=/home/alejandro/workspace/1-nasdaq-parser/build/ny4-xnas-tvitch-a-20230822-all-sorted.pcap,infinite_rx=0"

//
//   --no-huge       Use anonymous mmap instead of 2 MB hugepages.
//                   Fine for PCAP replay; avoids hugepage configuration.
//
//   -l 4           Single lcore — this pipeline is entirely single-threaded.
//
//   --vdev          Register a virtual NIC backed by the .pcap file.
//                   infinite_rx=0 stops the PMD at EOF instead of looping.
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // EAL initialisation

    try {
        dpdk::init_eal(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // Validate that the vdev registered a port 
    //
    // If --vdev was missing or malformed, rte_eth_dev_count_avail returns 0
    // and every subsequent rte_eth_* call on port 0 would be undefined behaviour.

    if (dpdk::available_ports() == 0) {
        std::cerr << "[ERROR] No DPDK ports found.\n"
                  << "        Launch with: --vdev \"net_pcap0,rx_pcap=<file>,infinite_rx=0\"\n";
        dpdk::cleanup();
        return 1;
    }

    // Pool and port setup 
    //
    // PortConfig owns all tuning values (port_id, nb_mbufs, rx_desc, etc.).

    dpdk::PortConfig port_cfg;   // all defaults apply

    rte_mempool* pool = nullptr;
    try {
        pool = dpdk::create_mempool("NYSE_POOL", port_cfg.nb_mbufs);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        dpdk::cleanup();
        return 1;
    }

    // Runs the mandatory DPDK port state machine:
    //   rte_eth_dev_configure            → locks queue topology (1 RX, 0 TX)
    //   rte_eth_dev_adjust_nb_rx_tx_desc → clamps rx_desc to PMD limits
    //   rte_eth_rx_queue_setup           → allocates the descriptor ring
    //   rte_eth_dev_start                → opens the .pcap file handle, enables RX

    try {
        dpdk::setup_port(port_cfg, pool);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        dpdk::cleanup();
        return 1;
    }

    // PCAP PMD always reports link-up immediately; this guard is unnecesary for this use case
    if (!dpdk::wait_for_link(port_cfg.port_id)) {
        std::cerr << "[WARN] Port " << port_cfg.port_id
                  << " link did not come up within 2 s. Continuing anyway.\n";
    }

    // Binary dump 
    //
    // Writes raw MoldUdp frames to disk BEFORE any protocol parsing.

    std::unique_ptr<BinaryDump> dump;
    if (DUMP_PATH) {
        dump = std::make_unique<BinaryDump>(DUMP_PATH);
        std::cout << "[INFO] Binary dump → " << DUMP_PATH << "\n";
    }

    //  RX loop + scalar decoder
    //
    // run_rx_loop is a template function: inlined

    std::cout << "[INFO] Decoder:\n";
    std::cout << "[INFO] Starting RX loop — Ctrl+C to stop.\n";

    model::OrderBook book;

    itch::ScalarDecoder decoder(book);

    dpdk::run_rx_loop(port_cfg.port_id, decoder, dump.get(), g_stop);

    // print_stats(decoder.stats());
    decoder.msg_stats.report("Full ITCH Message Processing Latency");
    decoder.decode_stats.report("Just decode, and endian changes Processing Latency");
    decoder.book_stats.report("Book Order Processing Latency");

    book.print_summary();



    // Teardown

    dpdk::teardown_port(port_cfg.port_id);
    dpdk::cleanup();
    return 0;
}