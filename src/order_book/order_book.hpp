#pragma once
#include "decoder/mold_itch_protocol.hpp"
namespace itch {
  struct SymbolInfo {
    bool valid = false;
    char stock[9]{};
    MarketCategory market_category = MarketCategory::NotAvailable;
    FinancialStatusInd financial_status_indicator = FinancialStatusInd::NotAvailable;
    uint32_t round_lot_size = 0;
    RoundLotsOnly round_lots_only = RoundLotsOnly::NoRestriction;
};

}