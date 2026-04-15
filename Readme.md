A high-performance C++ pipeline for parsing NASDAQ TotalView-ITCH PCAP dumps, built to research and benchmark HFT latency optimizations:
- Network I/O: DPDK poll-mode processing vs. custom memory-mapped (mmap) ingestion.

- Decoding: MoldUDP64 transport parsing and ITCH message decoding.

- Compute: Scalar parsing vs. SIMD-vectorized decoding with cache-aware optimizations.