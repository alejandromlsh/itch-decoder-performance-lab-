#pragma once

#include <string>
#include <fstream>
#include <cstdint>

#include <rte_mbuf.h> 

// Changes to move to rx_loop
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_byteorder.h>
////// end of those changes

// constexpr uint16_t ETH_IP_UDP_LEN = 14 + 20 + 8;


class BinaryDump {
public:
    explicit BinaryDump(std::string path) : file_(path,std::ios::binary | std::ios::trunc){

    }
    ~BinaryDump(){
      if (file_.is_open()) {
        file_.flush();
      }
    }
    
    // void write_burst(rte_mbuf ** mbuf,uint16_t count) {
    void write_packet(const uint8_t * payload, uint16_t payload_len) {
    
        // for (int i = 0; i < count;++i) {
        //     rte_mbuf * this_mbuf = mbuf[i];

        //     // 1. Start at the Ethernet header
        //     uint8_t * data = rte_pktmbuf_mtod(this_mbuf, uint8_t *);
        //     uint16_t offset = sizeof(struct rte_ether_hdr);
        //     struct rte_ether_hdr * eth = reinterpret_cast<struct rte_ether_hdr *>(data);

        //     uint16_t ether_type = rte_be_to_cpu_16(eth->ether_type);

        //     // 2. Check for 802.1Q VLAN tag and adjust offset if present

        //     if (ether_type == RTE_ETHER_TYPE_VLAN) {
        //         struct rte_vlan_hdr * vlan = reinterpret_cast<struct rte_vlan_hdr *>(data + offset);
        //         ether_type = rte_be_to_cpu_16(vlan->eth_proto); // Get the real EtherType
        //         offset += sizeof(struct rte_vlan_hdr); 
        //     }

        //     // Only process IPv4 packets
        //     if (ether_type != RTE_ETHER_TYPE_IPV4) {
        //         continue; 
        //     }
        //     // 3. Parse IPv4 Header to get dynamic IP header length
        //     struct rte_ipv4_hdr * ip = reinterpret_cast<struct rte_ipv4_hdr *>(data + offset);

        //     // Only process UDP packets
        //     if (ip->next_proto_id != IPPROTO_UDP) {
        //         continue;
        //     }

        //     // ihl contains length in 32-bit words, multiply by 4 for bytes (usually 20 bytes)
        //     uint8_t ip_hdr_len = (ip->version_ihl & 0x0f) * 4; 
        //     offset += ip_hdr_len;

        //     // 4. Parse UDP Header
        //     struct rte_udp_hdr * udp = reinterpret_cast<struct rte_udp_hdr *>(data + offset);

        //     // DPDK dgram_len includes the 8-byte UDP header. Subtract 8 to get payload length.
        //     uint16_t payload_len = rte_be_to_cpu_16(udp->dgram_len) - sizeof(struct rte_udp_hdr);

        //     // Move offset past the UDP header
        //     offset += sizeof(struct rte_udp_hdr);

        //     // 5. Extract payload and write to file
        //     const uint8_t * payload = data + offset;
        //     file_.write(reinterpret_cast<const char *>(payload), payload_len);
          
        // }
        file_.write(reinterpret_cast<const char *>(payload),payload_len);


    }

private:
    std::string path_;
    std::ofstream file_;
};