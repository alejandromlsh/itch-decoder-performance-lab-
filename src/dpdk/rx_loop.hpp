#pragma once

#include <rte_ethdev.h>   // rte_eth_rx_burst
#include <rte_mbuf.h>     // rte_pktmbuf_free_bulk, rte_mbuf


#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_byteorder.h>

#include "io/binary_dump.hpp"

// #include <scalar_decoder.hpp>
inline constexpr bool kEnableDump = false;


namespace dpdk {

    // Maximum mbufs fetched in one rte_eth_rx_burst call.
    inline constexpr uint16_t RX_BURST_SIZE = 32;

    // How many consecutive zero-return bursts we tolerate before declaring
    // the PCAP file exhausted.
    inline constexpr int EOF_THRESHOLD = 20;

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

        // start timer
        uint64_t e2e_start = rte_rdtsc();

        while(!stop_flag) {
            const uint16_t count = rte_eth_rx_burst(port_id, 0, mbufs, RX_BURST_SIZE);

            if (count == 0) {
                ++empty_burst_count;   // PCAP EOF
                // printf("[INFO] rte_eth_rx_burst returned 0\n");
                // Only check the slow link status if we've spun 10 times
                if (empty_burst_count > EOF_THRESHOLD) {
                    break; // for testing I will avoid the next calls
                    struct rte_eth_link link;
                    rte_eth_link_get_nowait(port_id, &link);
                    
                    if (link.link_status == RTE_ETH_LINK_DOWN) {
                        // printf("\n[INFO] PCAP EOF detected (link down).\n");
                        break; 
                    }
                    empty_burst_count = 0; // Reset so we don't spam the check
                }
                continue;
            }
            empty_burst_count = 0;

            for (int i = 0; i < count;++i) {
                rte_mbuf * this_mbuf = mbufs[i];

                // 1. Start at the Ethernet header
                uint8_t * data = rte_pktmbuf_mtod(this_mbuf, uint8_t *);
                uint16_t offset = sizeof(struct rte_ether_hdr);
                struct rte_ether_hdr * eth = reinterpret_cast<struct rte_ether_hdr *>(data);

                uint16_t ether_type = rte_be_to_cpu_16(eth->ether_type);

                // 2. Check for 802.1Q VLAN tag and adjust offset if present

                if (ether_type == RTE_ETHER_TYPE_VLAN) {
                    struct rte_vlan_hdr * vlan = reinterpret_cast<struct rte_vlan_hdr *>(data + offset);
                    ether_type = rte_be_to_cpu_16(vlan->eth_proto); // Get the real EtherType
                    offset += sizeof(struct rte_vlan_hdr); 
                }

                // Only process IPv4 packets
                if (ether_type != RTE_ETHER_TYPE_IPV4) {
                    continue; 
                }
                // 3. Parse IPv4 Header to get dynamic IP header length
                struct rte_ipv4_hdr * ip = reinterpret_cast<struct rte_ipv4_hdr *>(data + offset);

                // Only process UDP packets
                if (ip->next_proto_id != IPPROTO_UDP) {
                    continue;
                }

                // ihl contains length in 32-bit words, multiply by 4 for bytes (usually 20 bytes)
                uint8_t ip_hdr_len = (ip->version_ihl & 0x0f) * 4; 
                offset += ip_hdr_len;

                // 4. Parse UDP Header
                struct rte_udp_hdr * udp = reinterpret_cast<struct rte_udp_hdr *>(data + offset);

                // DPDK dgram_len includes the 8-byte UDP header. Subtract 8 to get payload length.
                uint16_t payload_len = rte_be_to_cpu_16(udp->dgram_len) - sizeof(struct rte_udp_hdr);

                // Move offset past the UDP header
                offset += sizeof(struct rte_udp_hdr);

                // 5. Extract payload and write to file
                uint8_t * payload = data + offset;
                
                if (kEnableDump) [[unlikely]] {
                  dump->write_packet(payload,payload_len);
                }
                
                // decoder dispatch
                decoder.process_payload(payload,payload_len);
            }

            // free resources
            rte_pktmbuf_free_bulk(mbufs, count); // free in bulk instead of in a loop
            // rte_pktmbuf_free()

        }
        // We count here full processing time
        uint64_t e2e_end = rte_rdtsc();
        double e2e_seconds = (double)(e2e_end - e2e_start) / rte_get_tsc_hz();
        printf("\n[INFO] End-to-End Rx Loop Time: %.6f seconds\n", e2e_seconds);

    } //namespace dpdk
}