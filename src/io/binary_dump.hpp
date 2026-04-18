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
    
        file_.write(reinterpret_cast<const char *>(payload),payload_len);

    }

private:
    std::string path_;
    std::ofstream file_;
};