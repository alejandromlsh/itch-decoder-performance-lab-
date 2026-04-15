#include "eal_init.hpp"

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_version.h>

#include <stdexcept>

namespace dpdk {
  int init_eal(int argc, char** argv) {

      //  1. MEMORY MAPPING
      //     Allocates hugepage memory (2 MB pages by default) or falls back to
      //     anonymous mmap when --no-huge is passed.

      //  2. LCORE THREADS
      //     Spawns one pthread per logical core listed in --lcores / -l.
      //     We use "-l 0" (single core) because PCAP replay is single-threaded.
      //  3. PMD DRIVER REGISTRATION
      //     Scans the --vdev arguments and instantiates the matching driver.
      //     "--vdev net_pcap0,rx_pcap=file.pcap,infinite_rx=0" loads the PCAP
      //     poll-mode driver and binds it as port 0.
      //     "infinite_rx=0" tells the PMD to stop after the last packet in the
      //     file rather than cycling back to the beginning — essential for a
      //     single-pass decode run.

      int ret = rte_eal_init(argc,argv);

      if (ret < 0) {
          throw std::runtime_error(std::string("EAL failed to initialise: ") 
              + rte_strerror(rte_errno));
      }
      return ret;

  }

  int cleanup() {

    return rte_eal_cleanup();
  }
}

