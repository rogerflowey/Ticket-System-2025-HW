#include "order.h"

void OrderManager::record_order(const UsernameKey &user_key, const Order &order) {
  user_orders_.insert({user_key.hash(), order.command_ts}, order);
  if (order.status == OrderStatus::PENDING) {
    waitlist_.insert({order.train_hash, order.original_train_date},
                     WaitlistEntry{
                       order.command_ts, user_key.hash(),
                       order.from_station_idx, order.to_station_idx,
                       order.num_tickets
                     });
  }
}

sjtu::vector<WaitlistEntry> OrderManager::get_wait_list(const WaitlistKey &key) {
  return waitlist_.find(key);
}

std::string OrderManager::query_order(const UsernameKey &user_key) {
  std::ostringstream oss;
  auto result = user_orders_.find_range({user_key.hash(), 0}, {user_key.hash(), std::numeric_limits<int>::max()});
  oss << result.size() << '\n';
  while (!result.empty()){
    oss << result.back().second.format_for_query() << '\n';
    result.pop_back();
  }
  return oss.str();
}

bool OrderManager::update_order_status(const OrderKey& key, OrderStatus new_status) {
  return user_orders_.modify(key, [&](Order& order_in_bpt) {
      order_in_bpt.status = new_status;
  });
}

void OrderManager::remove_from_waitlist(const WaitlistKey& wk, const WaitlistEntry& entry_to_remove) {
  waitlist_.erase(wk, entry_to_remove);
}

std::optional<OrderManager::RefundableOrderInfo> OrderManager::get_nth_refundable_order(const UsernameKey& user_key, int n) {
    auto user_order_pairs = user_orders_.find_range({user_key.hash(), std::numeric_limits<int>::min()},
                                                   {user_key.hash(), std::numeric_limits<int>::max()});

    if (n <= 0 || static_cast<size_t>(n) > user_order_pairs.size()) {
        return std::nullopt;
    }

    int target_idx_in_ascending_list = user_order_pairs.size() - n;

    const auto& pair_to_refund = user_order_pairs[target_idx_in_ascending_list];
    return RefundableOrderInfo{pair_to_refund.second, pair_to_refund.first};
}

// New method implementation
int OrderManager::refund_order_for_user(const UsernameKey& user_key, int n, TrainManager& train_mgr) {
    auto refundable_order_info_opt = get_nth_refundable_order(user_key, n);

    if (!refundable_order_info_opt) {
        return -1; // Order not found or n out of bounds
    }

    RefundableOrderInfo& ref_info = refundable_order_info_opt.value();
    Order& order_to_refund = ref_info.order_data;
    OrderKey& order_key_to_refund = ref_info.key;

    if (order_to_refund.status == OrderStatus::REFUNDED) {
        return -1; // Already refunded
    }

    OrderStatus original_status = order_to_refund.status;

    if (!update_order_status(order_key_to_refund, OrderStatus::REFUNDED)) {
        return -1; // Failed to update status in DB
    }

    if (original_status == OrderStatus::SUCCESS) {
        std::string train_id_s = order_to_refund.train_id.c_str();
        std::string date_depart_s = order_to_refund.leave_time.getDateString();

        std::string from_station_s, to_station_s;
        if (order_to_refund.from_station_id < 0 || static_cast<size_t>(order_to_refund.from_station_id) >= station_id_to_name_vec.size() ||
            order_to_refund.to_station_id < 0 || static_cast<size_t>(order_to_refund.to_station_id) >= station_id_to_name_vec.size()) {
            std::cerr<<"Refund Data corrupt: Invalid station ID in order."<<std::endl;
            return -1;
            }
        from_station_s = station_id_to_name_vec[order_to_refund.from_station_id];
        to_station_s = station_id_to_name_vec[order_to_refund.to_station_id];
        std::string num_tickets_s = std::to_string(order_to_refund.num_tickets);
        train_mgr.process_refunded_tickets(*this, train_id_s, date_depart_s, from_station_s, to_station_s, num_tickets_s);

    } else if (original_status == OrderStatus::PENDING) {
        WaitlistKey wk = {order_to_refund.train_hash, order_to_refund.original_train_date};
        WaitlistEntry wle = {order_to_refund.command_ts, user_key.hash(),
                             order_to_refund.from_station_idx, order_to_refund.to_station_idx,
                             order_to_refund.num_tickets};
        remove_from_waitlist(wk, wle);
    }

    return 0; // Refund processed
}