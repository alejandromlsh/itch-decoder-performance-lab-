# NASDAQ TotalView-ITCH High-Performance Parser

Currently one-threaded high-performance C++ pipeline for parsing NASDAQ TotalView-ITCH PCAP dumps, built to research and benchmark HFT latency optimizations:

- Network I/O: DPDK poll-mode processing, goal of getting the packets with real networking and kernel bypass from PCIe.
- Decoding: MoldUDP64 transport parsing and ITCH message decoding.
- Compute & Data Structures: Profiling compiler optimizations (-O3, -march=native) and measuring the latency cost of standard C++ STL containers vs. custom zero-allocation structures as well as cache-aware optimisations.
- Order Book: Level 2 order book, with the goal of writing a Level 3 and improve the optimisations in it.

The goal is to optimise the pipeline, check different techniques, benchmark and compare them, and also grow it in completeness adding more functionalities such as book sequencer, multiplexer, strategy thread and lock-free synchronization mechanism (ring buffer if I keep it SPSC).

---

## Table of Contents
1. [Performance Summary (Tag v1.0.0)](#performance-summary-tag-v100)
   * [Conditions](#conditions)
   * [Latency Results](#latency-results)
   * [Analysis (Internal, perf stat, perf report)](#analysis)
2. [Roadmap](#roadmap)

---

## Performance Summary (Tag v1.0.0)

### Conditions
The conditions used for the benchmark are:
- Code pinning to one core that has been isolated from other tasks using the GRUB file.
- Number of cycles are computed using `rte_rdtsc()` and converted into time using `rte_get_tsc_hz()`.
- A PCAP file containing NASDAQ messages from the pre-opening of the session is used. It contains 399,621,195 ITCH messages.
- For the analysis, I used an internal counter feeding a histogram, and also `perf stat` and `perf report`.

### Latency Results

| Metric                                           | Without `-O3 -march=native` | With `-O3 -march=native` |
|-------------------------------------------------|-----------------------------|--------------------------|
| End-to-end RX loop wall time                    | 423.88 s                    | 257.05 s                |
| Total ITCH messages processed                   | 399,621,195                 | 399,621,195             |
| **Decode-only latency (avg)**                   | 10.89 ns                    | 11.21 ns                |
| Decode-only p50                                 | 10.02 ns                    | 10.02 ns                |
| Decode-only p90                                 | 10.02 ns                    | 10.02 ns                |
| Decode-only p99                                 | 30.06 ns                    | 50.10 ns                |
| **OrderBook update latency (avg)**              | 872.22 ns                   | 432.99 ns               |
| OrderBook p50                                   | 741.41 ns                   | 320.61 ns               |
| OrderBook p90                                   | 1,593.03 ns                 | 1,031.96 ns             |
| OrderBook p99                                   | 2,554.85 ns                 | 1,723.27 ns             |
| OrderBook p99.9                                 | 3,847.31 ns                 | 2,494.74 ns             |
| **Full ITCH processing latency (avg)**          | 899.77 ns                   | 478.13 ns               |
| Full ITCH p50                                   | 781.48 ns                   | 360.69 ns               |
| Full ITCH p90                                   | 1,633.10 ns                 | 1,082.06 ns             |
| Full ITCH p99                                   | 2,614.97 ns                 | 1,783.39 ns             |
| Full ITCH p99.9                                 | 3,937.48 ns                 | 2,594.93 ns             |

The average time for parsing, decoding and inserting a message into the book order is ~430 ns for the `-O3` optimized run. However, it is important to check the slow outliers since those are the ones that must be corrected first. 10% of the messages take more than 1000 ns, and 0.1% take more than 2500 ns. Some of the slow messages are likely not in the hot path, as the `StockTradingDirectory` messages, but the processing of the p99 is definitely too slow and must be corrected.

---

### Analysis

#### Using `Internal Benchmarking`:
Decoding a message takes around 10 ns while passing it to the book order takes around 430 ns. Clearly, the full message process is dominated by the BookOrder. The decoding time is just a fraction of it. Therefore, the first effort is to improve the design of the BookOrder, which currently uses `std::map` and `std::unordered_map`, causing a loss of time with allocations.

#### Using `perf stat`:
```text
task-clock:         256,141.95 msec   # 0.999 CPUs utilized
cycles:             1,023,298,986,323 # ~3.995 GHz
instructions:         527,395,652,867 # 0.52 insn per cycle
stalled-cycles-front:64,719,917,588   # 6.32% frontend cycles idle
branches:           101,774,564,563   # 397M/sec
branch-misses:        2,245,859,470   # 2.21% of branches
L1-dcache-loads:    278,195,602,471   # 1.086 G/sec
L1-dcache-load-miss:16,687,578,542    # 6.00% L1 miss rate
context-switches:          1,612      # very few
cpu-migrations:               2
page-faults:           100,223
```
- **0.999 CPU usage** indicates that the code is being run almost exclusively on one core, helped by the isolation of that core with GRUB. The remaining time is likely related to the BIOS and not solvable while running with a normal Linux laptop.
- **Instructions per cycle are low: 0.52 insn.** This indicates that the core has stalled a lot. In combination with the time spent in the BookOrder, this is likely the consequence of `std::map` and `std::unordered_map`. In those structures, there is a lot of pointer chasing and indirections, not a tight arithmetic loop.
- **Branch misses are low: 2.21%.** This is a good point, indicating that the while loop in the decoder is easy to predict.
- **L1 miss rate is high: 6.00%.** This relates to the IPC and is likely caused by the same issue with `std::map` and `std::unordered_map`.

#### Using `perf report`:
![Perf Report](perf_reports/perf_report_v1.0.0.png)

- Due to `-O3` aggressive optimization and hints in the code, almost all functions are inlined and disappear, so we see that the computation basically becomes `process_payload` and expends 60% of the time there.
- ~20% of the time is spent in the iterator in the hash map, so that is one more confirmation of what the internal benchmarking showed. But I got the clue that the problem is in the `std::unordered_map` and not in the `std::map`. In the perf report, it is clearly written `_Hashtable` and not `_Rb_tree` (the internal implementation of the `std::map`).
- The remaining red objects `[kernel.kallsyms]` are system calls related to the fact that we are using DPDK in poll mode to read a file from the hard drive. A real NIC would write directly into the RAM using DMA, so these system calls would disappear. For a future version, I plan to receive packets from an FPGA using PCIe and then test the code without this.

---

## Roadmap

I plan to apply the next changes, both to diminish the latency and to make this project more complex and representative of a real pipeline

1. **Zero-Allocation Order Book (Targeting `v1.1.0`)**
   * Replace `std::unordered_map` and maybe `std::map` (depending of the result of benchmarking after removing unordered_map) with pre-allocated flat hash maps and arrays to remove heap allocation and iterations that are currently eating a lot of cycles in hot path. 
2. **Level 3 BookOrder (Targeting `v.2.0.0`)**
   * Improve the BookOrder to create a level 3 one
3. **Symbol Multiplexer**
   * Introduce a routing layer to direct ITCH events to per-symbol order books,  in this way modeling a more realistic multi-instrument setup and preparing the possibility of thread-level parallelism.
4. **Sequencer & Recovery**
   * Implement a MoldUDP64/ITCH sequencer to track expected sequence numbers and detect packet drops/gaps.
5. **Lock-Free SPSC Queue**
   * Split the architecture into a Feed Handler thread (decode) and a Strategy thread, connected via a lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
6. **Hardware Integration (PCIe/FPGA)**
   * Replace the DPDK PCAP poll loop with a AWS setup where a dedicated core streams packets over PCIe to an FPGA  where I will introduce a bitstream to make it act like a mirror and send packets back to me, testing the software against a more real network DMA situation