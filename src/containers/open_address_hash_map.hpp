#pragma once
#include <cstdint>
#include <memory>
// The max number of active orders at any time is:
// [ANALYSIS]: Max number of active orders is -> 3013608

// I want a load factor for the hash table from 0.5 to 0.7

//  high performance (load = 0.5):
// capacity≈ 3,013,608 /0.5≈ 6,027,216
// next power of 2 is : 2 to the power of 23   -> 8,388,608

//  (load = 0.7)
// capacity = 3012608 /0.7 = 4,305,154
// nex power of 2 still 2 to the power of 23

// So basically that is the sizer we will use.
// I need to preallocate 8 million ORder Entry so aprox 200Mb sin ram taking into consideration size of order Entry. This is acceptabke

// With 8.4 Million stops for a maximum of 3 million active orders the load factor will be 0.36, very fast with the caveat of bigger memory usage.
// But so far 200 shoul not be an issue for the RAM

// The small load factor means that a lot of spaces are empty and in case of collision the probing is minised

// Linear probing


// FIRST VERSION OF THIS PREALLOCATE A VECTOR. Maybe later mmap and hugepages

// SECOND VERSION OF THIS, Robin hood hashing

// Third version
// std::make_unique with mmap using the MAP_HUGETLB flag to allocate this contiguous memory using Huge Pages (2MB or 1GB pages). 
// This massively reduces TLB (Translation Lookaside Buffer) misses during probing.





namespace containers {

  // T must have default constructor
  template <typename T>
  struct Bucket {
    uint64_t order_reference;
    T order{};
    int8_t dib = -1; // -1 is Empty, 0 means ideal slow. > 0 means probed
  };

  template <typename T>
  class OpenAddressHashMap {

    // Capacity chosen from measured peak active orders:
    // max_active_orders ~= 3,013,608
    // 2^23 buckets => comfortable load factor at peak.
    static constexpr std::size_t kCapacity = 1U << 23;
    static constexpr std::size_t kMask = kCapacity - 1;

    std::unique_ptr<Bucket<T>[]> buckets;
    std::size_t size_ = 0;

  public:
    
    OpenAddressHashMap() : buckets(std::make_unique<Bucket<T>[]>(kCapacity)) {}

    // Rule of 5
    OpenAddressHashMap(const OpenAddressHashMap & other) = delete;
    OpenAddressHashMap & operator=(const OpenAddressHashMap& other) = delete;
    OpenAddressHashMap(OpenAddressHashMap && other) noexcept = default;
    OpenAddressHashMap & operator=(OpenAddressHashMap && other) = default;
    ~OpenAddressHashMap() = default;

    // exposed member functions
    std::size_t size() const {return size_;}

    static constexpr std::size_t capacity()  {return kCapacity;}
    

    // linear probing
    T * find(uint64_t order_ref) {
      std::size_t idx = index_for(order_ref);
      int8_t search_dib = 0;

      // Iterate. If search dib > bucket dib we know the element is not in the table
      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T> &bucket = buckets[idx];
        
        if (bucket.dib == -1 || search_dib > bucket.dib) {
          // Either it's empty, or the element that is here is closer to its home 
          // than this iteration. Therefore, the element I search cannot be past this point.
          return nullptr;
        }

        if (bucket.order_reference == order_ref) {
          return &bucket.order;
        }

        idx = next_index(idx);
        ++search_dib;
      }
      return nullptr;

    }

    const T * find(uint64_t order_ref) const {
      std::size_t idx = index_for(order_ref);
      int8_t search_dib = 0;

      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T>& bucket = buckets[idx];

        if (bucket.dib == -1 || search_dib > bucket.dib) {
          return nullptr;
        }
        if (bucket.order_reference == order_ref) {
          return &bucket.order;
        }

        idx = next_index(idx);
        ++search_dib;
      }
      return nullptr;
    }

    // perfert forwarding for flexibility in calling this. OrderEntry is very tiny so no performance difference, but gives more options when calling
    template<typename U>
    std::pair<T*,bool> emplace(uint64_t order_ref,U && order_val) {
      size_t idx = index_for(order_ref);
      int8_t current_dib = 0;

      uint64_t insert_ref = order_ref;
      T insert_val = std::forward<U>(order_val);
      
      // Track where the original new element lands
      T * result_ptr = nullptr;

      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T>& bucket = buckets[idx];

        // Empty slot found
        if (bucket.dib == -1) {
          bucket.order_reference = insert_ref;
          bucket.order = std::move(insert_val);
          bucket.dib = current_dib;
          ++size_;

          // If result ptr is nullptr, there has been not swaps. The original element landed here
          if (result_ptr == nullptr) {
            result_ptr = &bucket.order;
          }
          return {result_ptr,true};
        }

        // if != -1 first check if this is the same elelemnt already inserted
        if (bucket.order_reference == order_ref) {
          return {&bucket.order,false}; // false because already exists, was not inserted
        }

        // Robin Hood swap
        if (current_dib > bucket.dib) {
          // First of all, before the first swap we must store the adress of the slot where we want to store the original element 
          // Because once is swaped, we continue moving other elements and lose track of it
          if (result_ptr == nullptr) {
            result_ptr = &bucket.order;
          }

          // Now swap
          std::swap(insert_ref,bucket.order_reference);
          std::swap(insert_val,bucket.order);
          std::swap(current_dib,bucket.dib);
        }

        idx = next_index(idx);
        ++current_dib; // keep track of this distance

      } // for

      return {nullptr,false}; // table completely full
    }


    bool erase(uint64_t order_ref)  {
      std::size_t idx = index_for(order_ref);
      int8_t search_dib = 0;
      bool found = false;

      // first search
      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T>& bucket = buckets[idx];

        // element not there
        if (bucket.dib == -1 || search_dib > bucket.dib ){ 
          return false;
        }

        // element there
        if (bucket.order_reference == order_ref) {
          found = true;
          break;
        }

        idx = next_index(idx);
        ++search_dib;
      }

      if (!found) {
        return false;
      }
      // second: the backward shift that include remove this element
      size_--;

      std::size_t next_idx = next_index(idx);
      while (buckets[next_idx].dib > 0) {
        // move next bucket into this
        buckets[idx].dib = buckets[next_idx].dib - 1; // -1 because we are one position closer
        buckets[idx].order = std::move(buckets[next_idx].order);
        buckets[idx].order_reference = buckets[next_idx].order_reference;

        idx = next_idx;
        next_idx = next_index(next_idx);
      }
      // mark the last slot that we have shifted backwards as empty
      buckets[idx].dib = -1;
      return true;
    }

  private:


    [[nodiscard]] static constexpr uint64_t my_hash(uint64_t  key) {
      key ^= key >> 30;
      key *= 0xbf58476d1ce4e5b9ULL;
      key ^= key >> 27;
      key *= 0x94d049bb133111ebULL;
      key ^= key >> 31;
      return key;
    }

    static constexpr size_t index_for(uint64_t key) {
      return static_cast<std::size_t>(my_hash(key) & kMask); // kMask is all 1 after 2 to the power of 23 so we apply the mask to only get numbers there
    }

  static constexpr size_t next_index(std::size_t index) {
    return (index + 1) & kMask;
  }


};
}


