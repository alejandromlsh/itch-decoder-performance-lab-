#pragma once

#include "mold_itch_protocol.hpp"
#include "order_book/order_book.hpp"
#include <cstring>
#include "perf/perf_stats.hpp"



// Zero cost Latency Timer using template specialization
constexpr bool ENABLE_DETAILED_PROFILING = true;

template<bool Enables>
struct LatencyTimer;

// Specialization true
template<>
struct LatencyTimer<true> {
  uint64_t start_time = 0;
  inline void start() {
    start_time = rte_rdtsc();
  }
  inline void record_to(PerfStats& stats) {
    stats.record(rte_rdtsc() - start_time);
  }
};
// Specialization false
template<>
struct LatencyTimer<false> {
  inline void start(){}
  inline void record_to(PerfStats& stats){}
};

namespace itch {

enum class MsgType : char {
  SystemEvent = 'S',
  StockDirectory = 'R',
  StockTradingAction = 'H',
  AddOrderNoMPID = 'A',
  AddOrderWithMPID = 'F',
  OrderExecuted = 'E',
  OrderCancel = 'X',
  OrderDelete = 'D',
  OrderReplace = 'U'
};
class ScalarDecoder {

public:
    PerfStats msg_stats;
    PerfStats decode_stats;
    PerfStats book_stats;

    ScalarDecoder(model::OrderBook& book) : order_book(book){}

    void init(){}
    void finalize(){}

    void process_payload(uint8_t * payload, uint16_t len) {

      const auto * mold = reinterpret_cast<const MoldUDP64Header *>(payload);
      uint16_t msg_count = rte_be_to_cpu_16(mold->message_count); // Big endyan to small endian.number of itch msg in this mold 

      uint8_t * p = payload + sizeof(MoldUDP64Header);
      uint8_t * end = payload + len;

      for (uint16_t msg = 0; msg < msg_count;++msg) {

        if (p + sizeof(MessageHeader) > end) {
          break;
        }

        const auto * msg_header = reinterpret_cast<const MessageHeader*>(p);
        uint16_t msg_len = rte_be_to_cpu_16(msg_header->message_length);
        p += sizeof(MessageHeader);

        if (p + msg_len > end) {
          break;
        }

        MsgType msg_type = static_cast<MsgType>(*(reinterpret_cast<char *>(p)));

        // Masure full message latency
        LatencyTimer<ENABLE_DETAILED_PROFILING> msg_timer;
        msg_timer.start();
        
        switch (msg_type) {
          case MsgType::SystemEvent: {
            if (msg_len < sizeof(SystemEventMessage)) {
                  break;
            }
            handleSystemEventMesg(*reinterpret_cast<const SystemEventMessage*>(p));
            break;
          }
          case MsgType::StockDirectory : {
            if (msg_len < sizeof(StockDirectoryMessage)) {
              break;
            }
            handleStockDirectoryMsg(*reinterpret_cast<const StockDirectoryMessage*>(p));
            break;
          }

          case MsgType::StockTradingAction: {
            if (msg_len < sizeof(StockTradingActionMessage)) {
              break;
            }
            handleStockTradingAction(*reinterpret_cast<const StockTradingActionMessage *>(p));
            break;
          }
          
          case MsgType::AddOrderNoMPID: {
            if (msg_len < sizeof(AddOrderNoMPIDMessage)){
              break;
            }
            handleAddOrderNoMPID(*reinterpret_cast<const AddOrderNoMPIDMessage *>(p));
            break;
          }

          case MsgType::AddOrderWithMPID: {
            if (msg_len < sizeof(AddOrderWithMPIDMessage)){
              break;
            }
            handleAddOrderWithMPID(*reinterpret_cast<const AddOrderWithMPIDMessage *>(p));
            break;
          }

          case MsgType::OrderCancel: {
            if (msg_len < sizeof(OrderCancelMessage)) {
              break;
            }
            handleOrderCancel(*reinterpret_cast<const OrderCancelMessage *>(p));
            break;
          }
          case MsgType::OrderDelete: {
            if (msg_len < sizeof(OrderDeleteMessage)) {
              break;
            }
            handleOrderDelete(*reinterpret_cast<const OrderDeleteMessage *>(p));
            break;
          }

          case MsgType::OrderExecuted: {
            if (msg_len < sizeof(OrderExecuteMessage)) {
              break;
            }
            handleOrderExecute(*reinterpret_cast<const OrderExecuteMessage *>(p));
            break;
          }
          case MsgType::OrderReplace: {
            if (msg_len < sizeof(OrderReplaceMessage)) {
              break;
            }
            handleOrderReplace(*reinterpret_cast<const OrderReplaceMessage *>(p));          
            break;
          }
          
          default: {
            break;
          }
        }


        msg_timer.record_to(msg_stats);

        p += msg_len;

      }

    }

private:
    model::OrderBook & order_book;

    inline void handleAddOrderNoMPID(const AddOrderNoMPIDMessage& ao_msg) __attribute__((always_inline)) {
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();

      const uint16_t stock_locate = rte_be_to_cpu_16(ao_msg.stock_locate);
      const uint64_t order_ref = rte_be_to_cpu_64(ao_msg.order_reference_number);
      const model::Side side = model::toSide(ao_msg.buy_sell_indicator);
      const uint32_t price = rte_be_to_cpu_32(ao_msg.price);
      const uint32_t shares = rte_be_to_cpu_32(ao_msg.shares);

      decoder_timer.record_to(decode_stats);
      book_timer.start();
      order_book.add_order(stock_locate,order_ref,side,price,shares);

      book_timer.record_to(book_stats);

    }

    inline void handleAddOrderWithMPID(const AddOrderWithMPIDMessage& ao_msg) __attribute__((always_inline)) {
      //TBD

    }

    inline void handleOrderExecute(const OrderExecuteMessage& oe_msg) __attribute__((always_inline)) {
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();
      const uint64_t order_ref = rte_be_to_cpu_64(oe_msg.order_reference_number);
      const uint32_t ex_shares = rte_be_to_cpu_32(oe_msg.executed_shares);
      decoder_timer.record_to(decode_stats);

      book_timer.start();
      order_book.execute_order(order_ref,ex_shares);
      book_timer.record_to(book_stats);
    }

    inline void handleOrderCancel(const OrderCancelMessage& oc_msg) __attribute__((always_inline)){
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();
      const uint64_t order_ref = rte_be_to_cpu_64(oc_msg.order_reference_number);
      const uint32_t cancelled_shares = rte_be_to_cpu_32(oc_msg.cancelled_shares);
      decoder_timer.record_to(decode_stats);

      book_timer.start();
      order_book.cancel_order(order_ref,cancelled_shares);
      book_timer.record_to(book_stats);
    }

    inline void handleOrderDelete(const OrderDeleteMessage& od_msg) __attribute__((always_inline)){
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();
      const uint64_t order_ref = rte_be_to_cpu_64(od_msg.order_reference_number);
      decoder_timer.record_to(decode_stats);

      book_timer.start();
      order_book.delete_order(order_ref);
      book_timer.record_to(book_stats);
    }

    inline void handleOrderReplace(const OrderReplaceMessage& or_msg) __attribute__((always_inline)) {
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();
      const uint64_t original_order_ref = rte_be_to_cpu_64(or_msg.original_order_reference_number);
      const uint64_t new_order_ref = rte_be_to_cpu_64(or_msg.new_order_reference_number);
      const uint32_t price = rte_be_to_cpu_32(or_msg.price);
      const uint32_t shares = rte_be_to_cpu_32(or_msg.shares);
      decoder_timer.record_to(decode_stats);

      book_timer.start();
      order_book.replace_order(original_order_ref,new_order_ref,price,shares);
      book_timer.record_to(book_stats);
    }

    inline void handleSystemEventMesg(const SystemEventMessage& se_msg) __attribute__((always_inline)) {
      // TBD.
    }

    // This message is not in the hoth path, they are distributed at the start of session.
    inline void handleStockDirectoryMsg(const StockDirectoryMessage& sd_msg) __attribute__((always_inline)) {
      LatencyTimer<ENABLE_DETAILED_PROFILING> decoder_timer;
      LatencyTimer<ENABLE_DETAILED_PROFILING> book_timer;

      decoder_timer.start();
      const uint16_t stock_locate = rte_be_to_cpu_16(sd_msg.stock_locate);
      const uint32_t round_lot_size = rte_be_to_cpu_32(sd_msg.round_lot_size);
      decoder_timer.record_to(decode_stats);

      book_timer.start();
      order_book.insert_to_stock_directory(stock_locate, 
                                          sd_msg.stock,
                                          sd_msg.market_category,
                                          sd_msg.financial_status_indicator,
                                          round_lot_size,
                                          sd_msg.round_lots_only);
      book_timer.record_to(book_stats);
    }

    inline void handleStockTradingAction(const StockTradingActionMessage & sta_msg) __attribute__((always_inline)) {
      //TBD
    }

};

} //namespace itch