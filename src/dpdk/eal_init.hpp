#pragma once

namespace dpdk {

    int init_eal(int arcg, char** argv);

    int cleanup();
    int get_nb_port();
}


