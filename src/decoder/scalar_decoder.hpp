#pragma once

#include "mold_itch_protocol.hpp"
#include "order_book/order_book.hpp"
#include <cstring>
// #include <iostream>

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
    ScalarDecoder(model::OrderBook& book) : orderBook(book){}

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

        p += msg_len;

      }

    }

private:
    // std::array<SymbolInfo,0xFFFF+1> locate_to_symbol_{};
    model::OrderBook & orderBook;

    void handleAddOrderNoMPID(const AddOrderNoMPIDMessage& ao_msg) {
      orderBook.add_order(rte_be_to_cpu_16(ao_msg.stock_locate),
                          rte_be_to_cpu_16(ao_msg.order_reference_number),
                          model::toSide(ao_msg.buy_sell_indicator),
                          rte_be_to_cpu_32(ao_msg.price),
                          rte_be_to_cpu_32(ao_msg.shares));
    }

    void handleAddOrderWithMPID(const AddOrderWithMPIDMessage& ao_msg) {

    }

    void handleOrderExecute(const OrderExecuteMessage& or_msg) {

    }

    void handleOrderCancel(const OrderCancelMessage& oc_msg) {

    }

    void handleOrderDelete(const OrderDeleteMessage& od_msg) {

    }

    void handleOrderReplace(const OrderReplaceMessage& or_msg) {

    }

    void handleSystemEventMesg(const SystemEventMessage& se_msg) {
      char type = se_msg.message_type;
      uint16_t stock_locate = rte_be_to_cpu_16(se_msg.stock_locate);
      EventCode event_code = se_msg.event_code;
      
    }

    void handleStockDirectoryMsg(const StockDirectoryMessage& sd_msg) {
        orderBook.insert_to_stock_directory(rte_be_to_cpu_16(sd_msg.stock_locate),
                                            sd_msg.stock,
                                            sd_msg.market_category,
                                            sd_msg.financial_status_indicator,
                                            rte_be_to_cpu_32(sd_msg.round_lot_size),
                                            sd_msg.round_lots_only);

    }

    void handleStockTradingAction(const StockTradingActionMessage & sta_msg){

    }

};

} //namespace itch