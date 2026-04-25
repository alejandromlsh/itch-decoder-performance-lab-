#pragma once
#include "decoder/mold_itch_protocol.hpp"
#include <array>
#include <map>
// #include <unordered_map>
#include <cstring>
#include <cinttypes>
#include "containers/open_address_hash_map.hpp"

// Basic Book Order level 2

using namespace itch;
namespace model {

  struct SymbolInfo {
    bool valid = false;
    char stock[9]{};
    MarketCategory market_category = MarketCategory::NotAvailable;
    FinancialStatusInd financial_status_indicator = FinancialStatusInd::NotAvailable;
    uint32_t round_lot_size = 0;
    RoundLotsOnly round_lots_only = RoundLotsOnly::NoRestriction;
  };

  enum class Side : uint8_t {
    Buy,
    Sell
  };

  struct OrderEntry {
    uint16_t stock_locate;
    Side side;
    uint32_t price = 0; // itch price. integer representing a real. 4 floating position. Divide by 10000
    uint32_t shares = 0;
    // we could add the timestamp but not necessary yet
  };

  // price -> agregate quantities. sience number of shares and executes shares is 4 bytes, better use 8 for aggregate
  struct BookSide {
    std::map<uint32_t,uint64_t> price_levels;

    // split to a separate class for modularity and future extension. 
    // Somethings can be cacheizadas asi. Por ejemplo volumen or max price.
    // Se pueden anadir metodos extra
  };
  struct SymbolBook {
    BookSide bids;
    BookSide asks;
  };

  inline Side toSide(itch::BuySellInd indicator) {
    return (indicator == BuySellInd::Buy) ? Side::Buy : Side::Sell;
  }

class OrderBook{
  static constexpr size_t MAX_STOCK_LOCATE = 0xFFFF - 1; // 2 bytes of uint16_t -1
  std::array<SymbolInfo,MAX_STOCK_LOCATE> symbols_by_locate{}; // stock to symbol info. Index is stock locate
  std::array<SymbolBook,MAX_STOCK_LOCATE> books_by_locate{}; // stock locate to BookState of Symbol (bids and asks per price)
  // std::unordered_map<uint64_t, OrderEntry> orders_by_reference_num{}; // order_reference to the stored order itself
  containers::OpenAddressHashMap<OrderEntry> orders_by_reference_num{};;



  std::map<uint32_t,uint64_t>& side_levels(uint16_t stock_locate,Side side) {
    return (side == Side::Buy) ? books_by_locate[stock_locate].bids.price_levels
                              : books_by_locate[stock_locate].asks.price_levels;
  }

  // Add new quantities to this market level (price)
  void add_quantity_at_price_level(uint16_t stock_locate, Side side, uint32_t price, uint32_t quantity) {
    auto & side_map = side_levels(stock_locate,side);
    side_map[price] += quantity; // add number of shares to this price level bid or ask
  }

  void reduce_quantity_at_price_level(uint16_t stock_locate, Side side, uint32_t price, uint32_t quantity) {
    auto & side_map = side_levels(stock_locate,side);
    // to reduce we need a bit more careful than to add
    auto it = side_map.find(price);
    if (it == side_map.end()) {
      //invalid reducction
      return;
    }
    if (quantity > it->second) {
      //invalid reductio 
      return;
    }
    
    // now reduce
    it->second -= quantity;
    // if quantity is 0. Delete this price level from the BookSide map
    if (it->second == 0) {
      side_map.erase(it); // at this point better to use the iterator to remove, since it is inmediate. And not search again with they key
    }

  }

  const char * symbol_name(uint16_t stock_locate) {
    return symbols_by_locate[stock_locate].valid ? symbols_by_locate[stock_locate].stock : "unkown";
  }

public:
  uint64_t active_orders = 0;
  uint64_t max_active_orders = 0;
  void update_max_orders(bool add) {
    if (add) {
      ++active_orders;
      max_active_orders = std::max(max_active_orders,active_orders);
    } else {
      --active_orders;
    }
  }
  void insert_to_stock_directory(uint16_t stock_locate,
                                const char * stock8,
                                MarketCategory mkt_cat,
                                FinancialStatusInd finan_sts_ind,
                                uint32_t round_lot_size,
                                RoundLotsOnly round_lots_only
                                // for the future
                                ){
    SymbolInfo & s = symbols_by_locate[stock_locate];
    std::memcpy(s.stock,stock8,8);
    s.stock[8] ='\0';
    s.valid = true; // it becomes valid now
    s.market_category = mkt_cat;
    s.financial_status_indicator = finan_sts_ind;
    s.round_lot_size = round_lot_size;
    s.round_lots_only = round_lots_only;
  }

  void add_order(uint16_t stock_locate,
                 uint64_t order_reference,
                 Side side,
                 uint32_t price, // remember uint representing a real. 4 floating
                 uint32_t shares){
      auto [it, inserted] = orders_by_reference_num.emplace(order_reference, OrderEntry{
        .stock_locate = stock_locate,
        .side = side,
        .price = price,
        .shares = shares
      });

      if (!inserted) {
        // inserted will be false, if the unordered map already contains a order with this
        // order reference. In that case we return to avoid duplicated in books
        ++stats.duplicate_adds;
        return;
      }
      update_max_orders(true);
      add_quantity_at_price_level(stock_locate,side,price,shares);
      ++stats.adds;
  } 

  void execute_order(uint64_t order_reference,
                     uint32_t executed_shares){
      // auto it = orders_by_reference_num.find(order_reference); // lookup in the hashmap
      // if (it == orders_by_reference_num.end()) {
      //   //unkown order has been executed
      //   ++stats.unknown_executes;
      //   return;
      // }
      // OrderEntry& ord = it->second;
      OrderEntry * ord_ptr = orders_by_reference_num.find(order_reference);
      OrderEntry& ord = *ord_ptr;
      if (ord_ptr == nullptr) {
        //   //unkown order has been executed
        ++stats.unknown_executes;
        return;
      }
      if (executed_shares > ord.shares) {
        ++stats.invalid_reductions;
        return;
      }
      reduce_quantity_at_price_level(ord.stock_locate,ord.side,ord.price,executed_shares);
      // important the order could have non executed shares, so reduce number
      ord.shares -= executed_shares;
      // if drop to zero remove order from the book as we did from the map of price levels
      if (ord.shares == 0) {
        // orders_by_reference_num.erase(it);
        orders_by_reference_num.erase(order_reference);
        update_max_orders(false);
      }
      ++stats.executes;

  }

  // Partial reduction of shares
  void cancel_order(uint64_t order_reference,uint32_t cancelled_shares){
    // auto it = orders_by_reference_num.find(order_reference);
    // if (it == orders_by_reference_num.end()){
    //   ++stats.unknown_cancels;
    //   //no order
    //   return;
    // }
    // // reduce quantities in the price level
    // OrderEntry & order = it->second;
    OrderEntry * ord_ptr = orders_by_reference_num.find(order_reference);

    if (ord_ptr == nullptr) {
      //   //unkown order has been executed
      ++stats.unknown_executes;
      return;
    }
    OrderEntry & order = *ord_ptr;


    if (cancelled_shares > order.shares) {
      ++stats.invalid_reductions;
      // not possible to cancel more shares that there is in the order
      return;
    }
    reduce_quantity_at_price_level(order.stock_locate,order.side,order.price,cancelled_shares);
    order.shares -= cancelled_shares;

    if (order.shares == 0){
      // orders_by_reference_num.erase(it);
      orders_by_reference_num.erase(order_reference);
      update_max_orders(false);
    }
    ++stats.cancels;
  }

  void delete_order(uint64_t order_reference){
    // auto it = orders_by_reference_num.find(order_reference);
    // if (it == orders_by_reference_num.end()){
    //   //order not present
    //   ++stats.unknown_deletes;
    //   return;
    // }
    // // delete numbers of shares from the price level
    // OrderEntry & order = it->second;
    OrderEntry * ord_ptr = orders_by_reference_num.find(order_reference);

    if (ord_ptr == nullptr) {
      //   //unkown order has been executed
      ++stats.unknown_executes;
      return;
    }
    OrderEntry & order = *ord_ptr;

    reduce_quantity_at_price_level(order.stock_locate,order.side,order.price,order.shares);
    // Now delete the order from the orders
    // orders_by_reference_num.erase(it);
    orders_by_reference_num.erase(order_reference);

    ++stats.deletes;
    update_max_orders(false);


  }
  void replace_order(uint64_t old_order_reference,
                     uint64_t new_order_reference,
                     uint32_t new_price,
                     uint32_t new_shares){

    // auto it_old = orders_by_reference_num.find(old_order_reference);
    // if (it_old == orders_by_reference_num.end()){
    //   ++stats.unknown_replaces;
    //   return;
    // }
    // OrderEntry old_order = it_old->second; // no reference because we need to copy
    // // delete old shares from price level
    OrderEntry * ord_ptr_old = orders_by_reference_num.find(old_order_reference);
    if (ord_ptr_old == nullptr) {
      ++stats.unknown_replaces;
      return;
    }
    OrderEntry old_order = *ord_ptr_old;

    reduce_quantity_at_price_level(old_order.stock_locate,old_order.side,old_order.price,old_order.shares);

    // orders_by_reference_num.erase(it_old);
    orders_by_reference_num.erase(old_order_reference);
    // now input new order

    auto [new_it,inserted] = orders_by_reference_num.emplace(new_order_reference,OrderEntry {
      .stock_locate = old_order.stock_locate,
      .side = old_order.side,
      .price = new_price,
      .shares = new_shares
    });

    if (!inserted) {
      // the new order is duplicated
      ++stats.duplicate_adds;
      return;
    }

    // Add new shares to the price level
    add_quantity_at_price_level(old_order.stock_locate,old_order.side,new_price,new_shares);
    ++stats.replaces;
  }

private:
  struct Stats {
    uint64_t adds = 0;
    uint64_t executes = 0;
    uint64_t cancels = 0;
    uint64_t deletes = 0;
    uint64_t replaces = 0;

    uint64_t duplicate_adds = 0;
    uint64_t unknown_executes = 0;
    uint64_t unknown_cancels = 0;
    uint64_t unknown_deletes = 0;
    uint64_t unknown_replaces = 0;
    uint64_t invalid_reductions = 0;
  };

  Stats stats{};

public:

  void print_summary() const {
      printf("\nadds=%" PRIu64 
            "\nexecutes=%" PRIu64
            "\ncancels=%" PRIu64
            "\ndeletes=%" PRIu64 
            "\nreplaces=%" PRIu64
            "\nactive_orders=%zu"
            "\nduplicate_adds=%" PRIu64
            "\nunknown_executes=%" PRIu64
            "\nunknown_cancels=%" PRIu64
            "\nunknown_deletes=%" PRIu64
            "\nunknown_replaces=%" PRIu64
            "\ninvalid_reductions=%" PRIu64
            "\n",
            stats.adds,
            stats.executes,
            stats.cancels,
            stats.deletes,
            stats.replaces,
            orders_by_reference_num.size(),              
            stats.duplicate_adds,
            stats.unknown_executes,
            stats.unknown_cancels,
            stats.unknown_deletes,
            stats.unknown_replaces,
            stats.invalid_reductions);
  }

  void print_top_of_book(uint16_t stock_locate) {
    const SymbolBook & stock_book = books_by_locate[stock_locate];

    printf("Symbol: %s  ",symbol_name(stock_locate));

    if (stock_book.bids.price_levels.empty()) {
      printf("BID=NA");
    } else {
      auto it = stock_book.bids.price_levels.rbegin(); // best bid is the maximum. Max price someone buys
      printf("BID = %" PRIu32 " | quantity = %" PRIu64 "  ", it->first, it->second);
    }

    if (stock_book.asks.price_levels.empty()) {
      printf("ASK=NA\n");
    } else {
      auto it = stock_book.asks.price_levels.begin(); // best ask is the minimm. Min price someone sell
      printf("ASK = %" PRIu32 " | quantity = %" PRIu64 "\n", it->first, it->second);
    }

  }

};

} // namespace 