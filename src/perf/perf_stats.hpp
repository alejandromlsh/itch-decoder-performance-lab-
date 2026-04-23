#pragma once

#include <vector>
#include <cstdio>
#include <cinttypes>

#include <rte_cycles.h>



// I use a histogram where each index is a different number
// of cycles. Each time simply increase the corresponding one
struct PerfStats {
    // 2,000,000 cycles at 3GHz = ~666 microseconds. 
    static constexpr uint64_t MAX_CYCLES = 2000000; 

    std::vector<uint64_t> histogram;
    
    uint64_t total_samples = 0;
    uint64_t overflows = 0;

    PerfStats() : histogram(MAX_CYCLES,0){}

    // In the hot path

    inline __attribute__((always_inline)) void record(uint64_t cycles) {
      ++total_samples;


      if (__builtin_expect(cycles < MAX_CYCLES,1)){
        ++histogram[cycles];
      } else {
        ++overflows;
      }

    }

    // in the cold path
    void report(const char* name) {
        if (total_samples == 0) return;

        uint64_t min_c = 0;
        uint64_t max_c = 0;
        uint64_t total_cycles = 0;

        // The histogram already clasify in a order way the amount of samples
        // Thats why getting percentiles is so simple
        uint64_t p50_target = total_samples * 0.50;
        uint64_t p90_target = total_samples * 0.90;
        uint64_t p99_target = total_samples * 0.99;
        uint64_t p999_target = total_samples * 0.999;

        uint64_t p50_c = 0, p90_c = 0, p99_c = 0, p999_c = 0;
        uint64_t cumulative = 0;

        for (uint64_t i = 0; i < MAX_CYCLES; ++i) {
            uint64_t count = histogram[i];
            if (count > 0) {
                if (min_c == 0) min_c = i; // First non-zero index is our min
                max_c = i;                 // Keep updating max to the latest non-zero
                total_cycles += (i * count); // Accumulate cycles for the average
                
                cumulative += count;
                if (!p50_c && cumulative >= p50_target) p50_c = i;
                if (!p90_c && cumulative >= p90_target) p90_c = i;
                if (!p99_c && cumulative >= p99_target) p99_c = i;
                if (!p999_c && cumulative >= p999_target) p999_c = i;
            }
        }

        double to_ns = 1e9 / rte_get_tsc_hz();
        double avg_c = (double)total_cycles / total_samples;

        printf("\n--- %s ---\n", name);
        printf("Messages : %" PRIu64 "\n", total_samples);
        printf("Min      : %.2f ns\n", min_c * to_ns);
        printf("Avg      : %.2f ns\n", avg_c * to_ns);
        printf("p50      : %.2f ns\n", p50_c * to_ns);
        printf("p90      : %.2f ns\n", p90_c * to_ns);
        printf("p99      : %.2f ns\n", p99_c * to_ns);
        printf("p99.9    : %.2f ns\n", p999_c * to_ns);
        
        if (overflows > 0) {
            printf("Max      : > %.2f ns (Overflow)\n", MAX_CYCLES * to_ns);
            printf("WARNING  : %" PRIu64 " samples exceeded histogram capacity.\n", overflows);
        } else {
            printf("Max      : %.2f ns\n", max_c * to_ns);
        }
    }

};

