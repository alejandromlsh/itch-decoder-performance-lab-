#include "port_manager.hpp"

#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_errno.h>

#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>

#define CACHE_SIZE 256
// as a rule of thumb nb_mbuf / 32 will always be in the limits of dpdk 
// cache * 1.5 <= nb_mbuf

namespace dpdk {

    rte_mempool * create_mempool(const char * name,uint32_t nb_mbufs) {

      // rte_pktmbuf_pool_create() carves a fixed-size ring of mbufs out of the
      // memory region mapped by rte_eal_init (hugepages or --no-huge anonymous
      // mmap).  
      // Each slot pre-allocates a contiguous data buffer that will hold
      // one packet's worth of bytes.


      //  RTE_MBUF_DEFAULT_BUF_SIZE  (= RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM)
      //                  = 2048 + 128 = 2176 bytes per slot.
      //                  NYSE ITCH/OUCH UDP payloads are at most ~1500 bytes

      //  SOCKET_ID_ANY   Allocate on whichever NUMA n(in a no server computer just 1) node has free memory.
      //                  On a single-socket workstation this is always node 0.
      //                  On multi-socket servers, align this with the NIC's socket
      //                  to avoid cross-socket memory traffic.

        rte_mempool * pool = rte_pktmbuf_pool_create(name,     //  Unique string identifier for the pool.
                                nb_mbufs, // Total number of mbuf slots in the ring.
                                CACHE_SIZE, //Per-lcore cache size.  Each lcore keeps a private ring of 256 mbufs 
                                0, //Private data size per mbuf (bytes appended after the rte_mbuf header)
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                SOCKET_ID_ANY);
        if (!pool) {
            throw std::runtime_error(std::string("[PORT] rte_pktmbuf_pool_create failed: ") +
                                                  rte_strerror(rte_errno));
    }
    return pool;
      
    }

    void setup_port(const PortConfig& config,rte_mempool * mem_pool) {
        
        // guard
        if (!rte_eth_dev_is_valid_port(config.port_id)) {
          throw std::runtime_error("[PORT] Port " + std::to_string(config.port_id) +
            " does not exist. Was --vdev net_pcap0,rx_pcap=<file> passed to EAL?");
        }

        // configure port
        rte_eth_conf port_conf{};

        int ret = rte_eth_dev_configure(config.port_id,
                                    config.nb_rx_queues,
                                    0,
                                    &port_conf);

        if (ret < 0) {
            throw std::runtime_error(
            "[PORT] rte_eth_dev_configure failed: " + std::string(rte_strerror(-ret)));
        }


        // Only needed for real NICs, not for PMD. Adjust limits and aligment
        uint16_t nb_rx_desc = config.rx_desc;
        uint16_t nb_tx_desc = 0;
        rte_eth_dev_adjust_nb_rx_tx_desc(config.port_id, &nb_rx_desc, &nb_tx_desc);

        // Now lets allocate the Rx descriptor ring for queue 0 (for PMD we will only have 1 queue).
        // Otherwise this would be a loop

        ret = rte_eth_rx_queue_setup(config.port_id,
                                        0,
                                      nb_rx_desc,
                                      rte_eth_dev_socket_id(config.port_id), //Returns the NUMA node the port is attached to
                                      nullptr,
                                    mem_pool);

        if (ret < 0) {
        throw std::runtime_error(
            "[PORT] rte_eth_rx_queue_setup failed: " + std::string(rte_strerror(-ret)));
        }
        
        // Transition from start to running
        ret = rte_eth_dev_start(config.port_id);
        if (ret < 0) {
            throw std::runtime_error(
                "[PORT] rte_eth_dev_start failed: " + std::string(rte_strerror(-ret)));
        }

    }

    bool wait_for_link(uint16_t port_id, int timeout_ms) {
        using clock = std::chrono::steady_clock;
        const auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);

        rte_eth_link link{};

        while (clock::now() < deadline) {
            rte_eth_link_get_nowait(port_id, &link);
            if (link.link_status == RTE_ETH_LINK_UP){
                return true;
            } 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    void teardown_port(uint16_t port_id) {
         rte_eth_dev_stop(port_id);

         rte_eth_dev_close(port_id);
    }

    uint16_t available_ports() {
    // rte_eth_dev_count_avail() counts all ports in a usable state.
    // "Usable" means: registered by a PMD and not yet closed.
    // After EAL init with --vdev net_pcap0,...  this returns 1.
    // Returns 0 if --vdev was missing or malformed.
    return rte_eth_dev_count_avail();
  }

} // dpdk