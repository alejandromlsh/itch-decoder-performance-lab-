#pragma once

#include <rte_ethdev.h>   // rte_eth_rx_burst
#include <rte_mbuf.h>     // rte_pktmbuf_free_bulk, rte_mbuf



#include "io/binary_dump.hpp"

namespace dpdk {

    // Maximum mbufs fetched in one rte_eth_rx_burst call.
    inline constexpr uint16_t RX_BURST_SIZE = 32;

    // How many consecutive zero-return bursts we tolerate before declaring
    // the PCAP file exhausted.
    inline constexpr int EOF_THRESHOLD = 3;

    // run_rx_loop<DecoderT>
    //
    // Template parameter DecoderT must provide:
    //   void process_burst(rte_mbuf** mbufs, uint16_t count)

    template<typename DecoderT>
    void run_rx_loop(uint16_t port_id,
                      DecoderT& decoder,
                      BinaryDump * dump,
                      const volatile bool& stop_flag) {
    
        rte_mbuf* mbufs[RX_BURST_SIZE]; // array of pointers
        int       empty_burst_count = 0;

        while(!stop_flag) {
            const uint16_t count = rte_eth_rx_burst(port_id, 0, mbufs, RX_BURST_SIZE);

            if (count == 0) {
                if (++empty_burst_count >= EOF_THRESHOLD) break;   // PCAP EOF
                continue;
            }
            empty_burst_count = 0;

            // same changes I add to the binary dumb are required before sending data to the decoder
            // so right now they will remain in the binary dumb, but they should be placed here.
            

            // if we set dump we will write to a binary file the payloads
            if (dump) {
                dump->write_burst(mbufs, count);
            }

            // decoder dispatch
            //decoder.process_burst(mbufs, count);

            // free resources
            rte_pktmbuf_free_bulk(mbufs, count); // free in bulk instead of in a loop
            // rte_pktmbuf_free()?

        }

    } //namespace dpdk
}