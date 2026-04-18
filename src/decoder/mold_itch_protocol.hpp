#pragma once
#include <cstdint>

namespace itch {
  // System
    enum class  EventCode : char {
      StartOfMesseges = 'O',
      StartOfSystemHours = 'S',
      StartOfMarketHours = 'Q',
      EndOfMarketHours = 'M',
      EndOfSystemHours = 'E',
      EndOfMessages = 'C'
    };

    // Stock Related
    enum class MarketCategory : char {
      NDAQGlobalSelectMrkt = 'Q',
      NDAQGlobalMrkt = 'G',
      NDAQCapitalMrkt = 'S',
      NYSE = 'N',
      NYSEMrkt = 'A',
      NYSEArca = 'P',
      BATSZExchange = 'Z',
      NotAvailable = ' '
    };

    enum class FinancialStatusInd : char {
      Deficient = 'D',
      Delinquent = 'E',
      Bankrupt = 'Q',
      Suspended = 'S',
      DeficientBankrupt = 'G',
      DeficientDelinquent = 'H',
      DelinquentBankrupt = 'J',
      DeficientDelinquentBankrupt = 'K',
      CreationsRedepmtionsSuspended = 'C',
      Normal = 'N',
      NotAvailable = ' '
    };

    enum class RoundLotsOnly : char {
      Yes = 'Y',
      NoRestriction = 'N'
    };

    enum class Authenticity : char {
      Production = 'P',
      Test = 'T'
    };

    enum class ShortSaleThreshold : char {
      Restricted = 'Y',
      NotRestricted = 'N',
      NotAvailable = ' '
    };

    enum class IPOFlag : char {
      Yes = 'Y',
      No = 'N',
      NotAvailable = ' '
    };

    enum class LULDreference : char {
      Tier1 = '1',
      Tier2 = '2',
      NotAplicable = ' '
    };

    enum class ETPflag : char {
      IsETP = 'Y',
      IsNotETP = 'N',
      NotAplicable = ' '
    };

    enum class InverseInd : char {
      IsInverseETP = 'Y',
      IsNotInverseETP = 'N'
    };

    //Trading action
    enum class TradingState : char {
      Halted = 'H',
      Paused = 'P',
      QuotationOnly = 'Q',
      Trading = 'T'
    };

    // Orders related
    enum class BuySellInd : char {
      Buy = 'B',
      Sell = 'S'
    };
    
}

#pragma pack(push,1)
namespace itch {

    // Mold wrapper
    struct MoldUDP64Header {
      char session[10];
      uint64_t sequence_number;
      uint16_t message_count;
      // message block
    };
    
    // Itch messages
    struct MessageHeader {
        uint16_t message_length;
    };

    struct CommonFieldsMessage {
        char message_type;
        uint16_t stock_locate;
        uint16_t tracking_number;
        uint8_t timestamp_ns[6]; // Remember endianess here
    };

    struct SystemEventMessage : CommonFieldsMessage {
        EventCode event_code;
    };
    // stock info
    struct StockDirectoryMessage : CommonFieldsMessage {
        char stock[8];
        MarketCategory market_category;
        FinancialStatusInd financial_status_indicator;
        uint32_t round_lot_size;
        RoundLotsOnly round_lots_only; 
        char issue_classification;
        char issue_subtype[2];
        Authenticity authenticity;
        ShortSaleThreshold short_sale_threshold_indicator;
        IPOFlag ipo_flag;
        LULDreference LULD_reference_price_tier;
        ETPflag etp_flag;
        uint32_t etp_leverage_factor;
        InverseInd inverse_indicator;
    };

  struct StockTradingActionMessage : CommonFieldsMessage {
    char stock[8];
    TradingState trading_state;
    char reserved;
    char reason[4];
  };
    //

  struct AddOrderNoMPIDMessage : CommonFieldsMessage {
    uint64_t order_reference_number;
    BuySellInd buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
  };

  struct AddOrderWithMPIDMessage : AddOrderNoMPIDMessage {
    char atrib[4];
  };


  struct OrderExecuteMessage : CommonFieldsMessage {
    uint64_t order_reference_number;
    uint32_t executed_shares;
    uint64_t match_number;
  };

  struct OrderCancelMessage : CommonFieldsMessage {
      uint64_t order_reference_number;
      uint32_t cancelled_shares;
  };

  struct OrderDeleteMessage : CommonFieldsMessage {
      uint64_t order_reference_number;
  };

  struct OrderReplaceMessage : CommonFieldsMessage {
      uint64_t original_order_reference_number;
      uint64_t new_order_reference_number;
      uint32_t shares; 
      uint32_t price; // Integer representing real number with 4 floating positions. Divide by 10000
  };
  //

}

#pragma pack(pop)

constexpr uint32_t mold_udpheader_size = 11;
constexpr uint32_t common_field_size = 11;
constexpr uint32_t system_event_size = 12;
constexpr uint32_t stock_dir_size = 39;
constexpr uint32_t stock_trading_Action_size = 25;
constexpr uint32_t addorder_nompid_size = 36;
constexpr uint32_t addorder_withmpid_size = 40;
constexpr uint32_t order_execute_size = 31;
constexpr uint32_t order_cancel_size = 23;
constexpr uint32_t order_delete_size = 19;
constexpr uint32_t order_replace = 35;


static_assert(sizeof(itch::CommonFieldsMessage) == common_field_size);
static_assert(sizeof(itch::SystemEventMessage) == system_event_size);
static_assert(sizeof(itch::StockDirectoryMessage) == stock_dir_size);
static_assert(sizeof(itch::StockTradingActionMessage) == stock_trading_Action_size);
static_assert(sizeof(itch::StockDirectoryMessage) == stock_dir_size);
static_assert(sizeof(itch::AddOrderNoMPIDMessage) == addorder_nompid_size);
static_assert(sizeof(itch::AddOrderWithMPIDMessage) == addorder_withmpid_size);
static_assert(sizeof(itch::OrderExecuteMessage) == order_execute_size);
static_assert(sizeof(itch::OrderCancelMessage) == order_cancel_size);
static_assert(sizeof(itch::OrderDeleteMessage) == order_delete_size);
static_assert(sizeof(itch::OrderReplaceMessage) == order_replace);




