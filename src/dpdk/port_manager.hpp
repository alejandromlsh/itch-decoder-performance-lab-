#pragma once

#include <cstdint>

struct rte_mempool; // forward-declare — avoids pulling all DPDK headers into
                    // every TU that includes this file


namespace dpdk {
    struct PortConfig {

        uint16_t port_id = 0;
        uint16_t nb_rx_queues = 1; //pollmode
        uint16_t rx_burst_size = 32; // used by rx_loop, 
        uint16_t rx_desc = 1024; //// depth of the per-queue descriptor ring
        uint32_t nb_mbufs = 8191; // 2^n -1

    };

    // Allocate a DPDK packet-buffer memory pool backed by hugepage / --no-huge memory.
    // Every mbuf returned by rte_eth_rx_burst comes from this pool.
    // Throws std::runtime_error on failure.
    rte_mempool * create_mempool(const char * name,uint32_t nb_mbufs);

    // Configure the Ethernet device, create the RX queue, and start reception.
    // Sequence: rte_eth_dev_configure → rte_eth_dev_adjust_nb_rx_tx_desc
    //           → rte_eth_rx_queue_setup → rte_eth_dev_start
    // Throws std::runtime_error on any step failure.
    void setup_port(const PortConfig & config,rte_mempool * mem_pool);

    // Poll link state until UP or timeout.
    // PCAP PMD always reports UP immediately.
    bool wait_for_link(uint16_t port_id, int timeout_ms = 1000);


    // Stop and release all port resources.
    // Call before cleanup_eal().
    void teardown_port(uint16_t port_id);

    // Poll link state until UP or timeout.
    // PCAP PMD always reports UP immediately.
    uint16_t available_ports();

}

