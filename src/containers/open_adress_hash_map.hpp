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


// FIRST VERSION OF THIS PREALLOCATE A VECTOR

// SECOND VERSION OF THIS, mmap and hugepages
namespace containers {

  enum class State : uint8_t {
    Empty,
    Occupied,
    Tombstone // slot that is "free to insert" but part of a chain that end in another occupied slot
  };
  // T must have default constructor
  template <typename T>
  struct Bucket {
    uint64_t order_reference;
    T order{};
    State state = State::Empty;
  };

  template <typename T>
  class OpenAdressHashMap {

    // Capacity chosen from measured peak active orders:
    // max_active_orders ~= 3,013,608
    // 2^23 buckets => comfortable load factor at peak.
    static constexpr std::size_t kCapacity = 1U << 23;
    static constexpr std::size_t kMask = kCapacity - 1;

    std::unique_ptr<Bucket<T>[]> buckets;
    std::size_t size_ = 0;

  public:
    
    OpenAdressHashMap() : buckets(std::make_unique<Bucket<T>[]>(kCapacity)) {}

    // Rule of 5
    OpenAdressHashMap(const OpenAdressHashMap & other) = delete;
    OpenAdressHashMap & operator=(const OpenAdressHashMap& other) = delete;
    OpenAdressHashMap(OpenAdressHashMap && other) noexcept = default;
    OpenAdressHashMap & operator=(OpenAdressHashMap && other) = default;
    ~OpenAdressHashMap() = default;

    // exposed member functions
    std::size_t size() const {return size_;}

    static constexpr std::size_t capacity()  {return kCapacity;}
    

    // linear probing
    T * find(uint64_t order_ref) {
      std::size_t idx = index_for(order_ref);

      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T> & bucket = buckets[idx];

        if (bucket.state == State::Empty) {
          return nullptr;
        }
        if (bucket.state == State::Occupied && bucket.order_reference == order_ref) {
          return &bucket.order;
        }
        idx = next_index(idx);
      }
      return nullptr;
    }
    const T * find(uint64_t order_ref) const {
      std::size_t idx = index_for(order_ref);

      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T> &bucket = buckets[idx];

        if (bucket.state == State::Empty) {
          return nullptr;
        }
        if (bucket.state == State::Occupied && bucket.order_reference == order_ref) {
          return &bucket.order;
        }
        idx = next_index(idx);
      }
      return nullptr;
    }

    // perfert forwarding for flexibility in calling this. OrderEntry is very tiny so no performance difference, but gives more options when calling
    template<typename U>
    std::pair<T*,bool> emplace(uint64_t order_ref,U && order_val) {
      std::size_t idx = index_for(order_ref);
      std::size_t first_tombstone = kCapacity; // the furthes away at the beginning

      for (std::size_t probe = 0; probe < kCapacity;++probe) {
        Bucket<T> & bucket = buckets[idx];

        if (bucket.state == State::Occupied) {
          if (bucket.order_reference == order_ref) {
            // we already inserted this order

            return std::make_pair(&bucket.order,false);
          }

        } else if (bucket.state == State::Tombstone) {
          // must try the full probing chain before inserting, to be sure the element is not later in the chain. simply update first tombstone
          if (first_tombstone == kCapacity) {
            first_tombstone = idx;
          }

        } else { // State == Empty
          std::size_t target = first_tombstone != kCapacity ? first_tombstone : idx;
          buckets[target].order_reference = order_ref;
          buckets[target].order = std::forward<U>(order_val);
          buckets[target].state = State::Occupied;
          ++size_;

          return std::make_pair(&buckets[target].order,true);
        }
        idx = next_index(idx);
      }

      
      // Case where there is no empty slots. By design it should not happens with the current data, since the size of the underlying vector
      // was selected and the load factor is 0.36 in worst case of the day but just in case
      if (first_tombstone != kCapacity) {
        buckets[first_tombstone].order_reference = order_ref;
        buckets[first_tombstone].order = std::forward<U>(order_val);
        buckets[first_tombstone].state = State::Occupied;
        ++size_;
        return {&buckets[first_tombstone].order, true};
      }
      // everything full, no empty or tombstone. 
      return {nullptr, false};
    }


    bool erase(uint64_t order_ref)  {
      std::size_t idx = index_for(order_ref);

      for (std::size_t probes = 0; probes < kCapacity; ++probes) {
        Bucket<T>& bucket = buckets[idx];

        if (bucket.state == State::Empty) {
          return false;
        }

        if (bucket.state == State::Occupied && bucket.order_reference == order_ref) {
          bucket.state = State::Tombstone;
          --size_;
          return true;
        }

        idx = next_index(idx);
      }

      return false;
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


