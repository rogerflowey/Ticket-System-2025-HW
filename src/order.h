#pragma once

#include <string>
#include "utils.h"       // For RFlowey::string or other utilities
#include "database.h"    // For OrderedHashMap, SingleMap
#include "datetime.h"
#include "common.h"      // For UsernameKey, TrainID_t, hash_t (if not in utils)
#include "user.h"
#include "train.h"

enum class OrderStatus {
  SUCCESS,
  PENDING,
  REFUNDED
};

struct Order {
  int command_ts;
  OrderStatus status;
  TrainID_t train_id;
  int from_station_id;
  int to_station_id;
  TimeUtil::DateTime leave_time;
  TimeUtil::DateTime arrive_time;
  int price_per_ticket;
  int num_tickets;

  hash_t train_hash;
  TimeUtil::DateTime original_train_date;
  Station_idx_t from_station_idx;
  Station_idx_t to_station_idx;

  Order() = default;

  Order(int ts, OrderStatus stat, const TrainID_t& tid,
        int fs_id, int ts_id,
        const TimeUtil::DateTime& lt, const TimeUtil::DateTime& at,
        int ppt, int nt,
        hash_t th, const TimeUtil::DateTime& otd, Station_idx_t fsi, Station_idx_t tsi)
      : command_ts(ts), status(stat), train_id(tid),
        from_station_id(fs_id), to_station_id(ts_id),
        leave_time(lt), arrive_time(at),
        price_per_ticket(ppt), num_tickets(nt),
        train_hash(th), original_train_date(otd), from_station_idx(fsi), to_station_idx(tsi) {}

  std::string format_status() const {
    switch (status) {
      case OrderStatus::SUCCESS: return "[success]";
      case OrderStatus::PENDING: return "[pending]";
      case OrderStatus::REFUNDED: return "[refunded]";
      default: return "[unknown]";
    }
  }

  std::string format_for_query() const {
    std::ostringstream oss;
    oss << format_status() << " "
        << std::string(train_id.c_str()) << " ";
    if (from_station_id >= 0 && static_cast<size_t>(from_station_id) < station_id_to_name_vec.size()) {
      oss << station_id_to_name_vec[from_station_id];
    } else { oss << "INVALID_ST_ID(" << from_station_id << ")"; }
    oss << " " << leave_time.getFullString() << " -> ";
    if (to_station_id >= 0 && static_cast<size_t>(to_station_id) < station_id_to_name_vec.size()) {
      oss << station_id_to_name_vec[to_station_id];
    } else { oss << "INVALID_ST_ID(" << to_station_id << ")"; }
    oss << " " << arrive_time.getFullString() << " "
        << price_per_ticket << " " << num_tickets;
    return oss.str();
  }
};

using WaitlistKey = RFlowey::pair<hash_t, TimeUtil::DateTime>;
struct WaitlistKeyHasher {
  hash_t operator()(const WaitlistKey& key) {
    return (key.first*33)^RFlowey::hash(key.second.getRawMinutes());
  }
};

struct WaitlistEntry {
    int command_ts;
    hash_t user_hash;
    int start_idx;
    int end_idx;
    int num_tickets_needed;

    static bool sortByTimestamp(const WaitlistEntry& a, const WaitlistEntry& b) {
        return a.command_ts < b.command_ts;
    }

  bool operator<(const WaitlistEntry& other) const {
      return command_ts < other.command_ts;
  }
  bool operator==(const WaitlistEntry& other) const {
      return command_ts == other.command_ts;
  }
};

using OrderKey = RFlowey::pair<hash_t,int>;

class OrderManager {
  SingleMap<OrderKey, Order> user_orders_;
  OrderedHashMap<WaitlistKey, WaitlistEntry,WaitlistKeyHasher> waitlist_;

public:
  constexpr static std::string db_path_prefix = "order_data";

  OrderManager() : user_orders_(db_path_prefix + "_user_orders.dat"),
                   waitlist_(db_path_prefix + "_waitlist.dat") {
  }

  void record_order(const UsernameKey& user_key,const Order& order);
  sjtu::vector<WaitlistEntry> get_wait_list(const WaitlistKey& key);
  std::string query_order(const UsernameKey &user_key);

  bool update_order_status(const OrderKey &key, OrderStatus new_status);

  void remove_from_waitlist(const WaitlistKey &wk, const WaitlistEntry &entry_to_remove);

  struct RefundableOrderInfo {
    Order order_data;
    OrderKey key;
  };
  std::optional<RefundableOrderInfo> get_nth_refundable_order(const UsernameKey& user_key, int n);

  // New method for refunding
  int refund_order_for_user(const UsernameKey& user_key, int n, TrainManager& train_mgr);

  void clear_data() {
    user_orders_.clear();
    waitlist_.clear();
  }
};