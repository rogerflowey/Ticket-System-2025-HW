#include "train.h"
#include "order.h"

TrainManager::TrainManager(): train_data_map_(db_path_prefix + ".dat"), seg_to_train_(db_path_prefix + "_seg.dat"),
                              daily_seat(db_path_prefix + "_seat.dat") {
}
int TrainManager::add_train(const std::string &train_id_str, const std::string &station_num_str,
                            const std::string &seat_num_str, const std::string &stations_str,
                            const std::string &prices_str, const std::string &start_time_str,
                            const std::string &travel_times_str, const std::string &stopover_times_str,
                            const std::string &sale_date_str, const std::string &type_str) {
  if (train_data_map_.find(train_id_str)) {
    return -1;
  };

  TrainData data;
  data.parse_arguments(train_id_str, station_num_str, seat_num_str, stations_str, prices_str, start_time_str,
                       travel_times_str, stopover_times_str, sale_date_str, type_str);
  data.release = false;
  train_data_map_.insert(data.train_id, data);
  return 0;
}

int TrainManager::delete_train(const std::string &train_id_str) {
  auto result = train_data_map_.find(train_id_str);
  if (!result) {
    return -1;
  }
  TrainData &train = result.value();
  if (train.release) {
    return -1;
  }
  train_data_map_.erase(train.train_id);
  return 0;
}

int TrainManager::release_train(const std::string &train_id_str) {
  auto result = train_data_map_.find(train_id_str);
  if (!result) {
    return -1;
  }
  TrainData &train = result.value();
  if (train.release) {
    return -1;
  }

  for (int i = 0; i < train.station_num; ++i) {
    for (int j = i + 1; j < train.station_num; ++j) {
      seg_to_train_.insert({train.stations[i], train.stations[j]},
                           {train.train_hash, {static_cast<Station_idx_t>(i), static_cast<Station_idx_t>(j)}});
    }
  }
  train_data_map_.modify_by_hash(train.train_hash, [&](TrainData& train_data) {
    train_data.release = true;
  });
  return 0;
}

int TrainManager::query_seat(const TrainData &train, Segment_t seg, TimeUtil::DateTime date) {
  auto from_idx = seg.second.first;
  auto to_idx = seg.second.second;
  int min_seat = train.seat_num;
  auto start_key = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, from_idx}
  };
  auto end_key = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, static_cast<Station_idx_t>(to_idx - 1)}
  };

  auto results = daily_seat.find_range(start_key, end_key);
  for (const auto &p: results) {
    min_seat = std::min(min_seat, p.second);
  }
  return min_seat;
}

bool TrainManager::reduce_seat(const TrainData &train,
                               Segment_t seg, const TimeUtil::DateTime &date,
                               int num_tickets) {
  auto from_idx = seg.second.first;
  auto to_idx = seg.second.second;
  auto key_start = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, from_idx}
  };
  auto key_end = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, static_cast<Station_idx_t>(to_idx - 1)}
  };

  auto existing_segments = daily_seat.find_range(key_start, key_end);

  size_t existing_ptr = 0;
  for (Station_idx_t current_s_idx = from_idx; current_s_idx < to_idx; ++current_s_idx) {
    if (existing_ptr < existing_segments.size() &&
        existing_segments[existing_ptr].first.second.second == current_s_idx) {
      existing_ptr++;
    } else {
      RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> > map_key_to_insert = {
        date, {train.train_hash, current_s_idx}
      };
      daily_seat.insert(map_key_to_insert, train.seat_num);
    }
  }

  daily_seat.range_modify(key_start, key_end, [&](int &seat_val) {
    seat_val -= num_tickets;
  });

  return true;
}

bool TrainManager::add_seat(const TrainData &train,
                            Segment_t seg,
                            const TimeUtil::DateTime &date,
                            int num_tickets) {
  auto from_idx = seg.second.first;
  auto to_idx = seg.second.second;
  auto key_start = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, from_idx}
  };
  auto key_end = RFlowey::pair<TimeUtil::DateTime, RFlowey::pair<hash_t, Station_idx_t> >{
    date, {train.train_hash, static_cast<Station_idx_t>(to_idx - 1)}
  };

  int modify_cnt = 0;
  daily_seat.range_modify(key_start, key_end, [&](int &seat_val) {
    int new_seat_num = seat_val + num_tickets;
    if (new_seat_num > train.seat_num) {
      throw std::runtime_error("Seat num exceed limit");
    }
    seat_val = new_seat_num;
    modify_cnt++;
  });
  if (modify_cnt != (to_idx - from_idx)) {
    throw std::runtime_error("Modify cnt not match, data corrupted");
  }
  return true;
}

std::string TrainManager::query_train(const std::string &train_id_str, const std::string &date_str) {
  TrainID_t train_id_key(train_id_str.c_str());
  auto train_data_opt = train_data_map_.find(train_id_key);
  if (!train_data_opt) {
    return "-1\n";
  }
  TrainData &train = train_data_opt.value();
  TimeUtil::DateTime date(date_str);
  if (!date.isValid() || !(train.sale_start <= date && date <= train.sale_end)) {
    return "-1\n";
  }
  std::ostringstream oss;
  oss << std::string(train.train_id.c_str()) << " " << train.type << "\n";

  TimeUtil::DateTime current_event_time = date;
  current_event_time = current_event_time + train.start_time;

  long long cumulative_price = 0;

  auto seat_result = daily_seat.find_range({date, {train.train_hash, 0}},
                                           {date, {train.train_hash, train.station_num - 1}});
  sjtu::vector<int> final_seat;
  final_seat.resize(train.station_num - 1, train.seat_num);
  for (auto &s: seat_result) {
    final_seat[s.first.second.second] = s.second;
  }
  for (size_t i = 0; i < train.station_num; ++i) {
    std::string station_name_str = "UNKNOWN_STATION";
    int current_station_id = train.stations[i];
    if (current_station_id >= 0 && static_cast<size_t>(current_station_id) < station_id_to_name_vec.size()) {
      station_name_str = station_id_to_name_vec[current_station_id];
    } else {
      station_name_str = "ERR_INV_ID_" + std::to_string(current_station_id);
    }

    std::string arrival_time_output_str;
    std::string leaving_time_output_str;
    std::string price_output_str;
    std::string seat_output_str;
    if (i == 0) {
      arrival_time_output_str = "xx-xx xx:xx";
      leaving_time_output_str = current_event_time.getFullString();
      price_output_str = "0";
      seat_output_str = std::to_string(final_seat[i]);
    } else {
      arrival_time_output_str = current_event_time.getFullString();
      cumulative_price += train.prices[i - 1];
      price_output_str = std::to_string(cumulative_price);

      if (i == train.station_num - 1) {
        seat_output_str = "x";
        leaving_time_output_str = "xx-xx xx:xx";
      } else {
        seat_output_str = std::to_string(final_seat[i]);
        current_event_time = current_event_time + train.stopover_times[i - 1];
        leaving_time_output_str = current_event_time.getFullString();
      }
    }
    oss << station_name_str << " " << arrival_time_output_str << " -> " << leaving_time_output_str << " " <<
        price_output_str << " " << seat_output_str << "\n";

    if (i < train.station_num - 1) {
      current_event_time = current_event_time + train.travel_times[i];
    }
  }
  return oss.str();
}


// In train.h, within TrainManager class or as a private helper struct


sjtu::vector<TrainManager::BaseTrainInfo> TrainManager::get_train_in_segment(RFlowey::pair<int, int> station_pair_key) {
  sjtu::vector<TrainManager::BaseTrainInfo> base_infos;
  auto segments_on_route = seg_to_train_.find(station_pair_key);

  for (const auto &seg_info: segments_on_route) {
    // seg_info is Segment_t: {train_hash, {idx_from, idx_to}}
    hash_t current_train_hash = seg_info.first;
    auto train_data_opt = train_data_map_.find_by_hash(current_train_hash);

    if (train_data_opt && train_data_opt.value().release) {
      base_infos.push_back({train_data_opt.value(), seg_info, {}});
    }
  }
  return base_infos;
}


void TrainManager::determine_date(sjtu::vector<TrainManager::BaseTrainInfo> &base, TimeUtil::DateTime min_depart_time) {
  for (auto &base_data: base) {
    TrainData &train = base_data.train;
    auto& seg_info = base_data.seg;
    Station_idx_t from_idx = seg_info.second.first;
    base_data.original_date = train.get_original_date(from_idx, min_depart_time);
  }
}

void TrainManager::filter_valid_date(sjtu::vector<TrainManager::BaseTrainInfo> &base) {
  sjtu::vector<TrainManager::BaseTrainInfo> new_data;
  for (auto &base_data: base) {
    auto &train = base_data.train;
    if (train.verify_date(base_data.original_date)) {
      new_data.push_back(base_data);
    };
  }
  base = new_data;
}

void TrainManager::filter_best_date(sjtu::vector<TrainManager::BaseTrainInfo> &base) {
  sjtu::vector<TrainManager::BaseTrainInfo> new_data;
  for (auto &base_data: base) {
    auto &train = base_data.train;
    auto new_date = train.find_earliest(base_data.original_date);
    if (new_date) {
      base_data.original_date = new_date.value();
      new_data.push_back(base_data);
    }
  }
  base = new_data;
}

sjtu::vector<QueryTicketInfo> TrainManager::process_output(sjtu::vector<TrainManager::BaseTrainInfo> &base) {
  sjtu::vector<QueryTicketInfo> found_tickets;
  for (auto &base_data: base) {
    auto& seg_info = base_data.seg;
    Station_idx_t from_idx = seg_info.second.first;
    Station_idx_t to_idx = seg_info.second.second;
    TrainData &train = base_data.train;
    int price = train.price_between(from_idx, to_idx);
    Time_t duration = train.time_between(from_idx, to_idx);
    const std::string &from_name = station_id_to_name_vec.at(train.stations[from_idx]);
    const std::string &to_name = station_id_to_name_vec.at(train.stations[to_idx]);
    int seats = query_seat(train, base_data.seg, base_data.original_date);

    found_tickets.push_back({
      std::string(train.train_id.c_str()),
      from_name,
      base_data.original_date + train.get_leave_time(from_idx),
      to_name, base_data.original_date + train.get_arrive_time(to_idx),
      price,
      seats,
      duration
    });
  }
  return found_tickets;
}


sjtu::vector<QueryTicketInfo> TrainManager::find_direct(RFlowey::pair<int, int> seg_key, TimeUtil::DateTime depart_date) {
  auto base = get_train_in_segment(seg_key);
  determine_date(base, depart_date);
  filter_valid_date(base);
  return process_output(base);
}

std::string TrainManager::query_ticket(const std::string &from_station_str, const std::string &to_station_str,
                                       const std::string &date_str, const std::string &sort_preference_str) {
  auto res_from = station_name_to_id(from_station_str);
  auto res_to = station_name_to_id(to_station_str);
  if(!res_from||!res_to) {
    return "0\n";
  }
  RFlowey::pair<int, int> seg_key = {res_from.value(),res_to.value()};
  TimeUtil::DateTime depart_date(date_str);
  auto found_tickets = find_direct(seg_key, depart_date);

  if (sort_preference_str == "time") {
    RFlowey::quick_sort(found_tickets.begin(), found_tickets.end(), QueryTicketInfo::sortByTime);
  } else {
    RFlowey::quick_sort(found_tickets.begin(), found_tickets.end(), QueryTicketInfo::sortByCost);
  }

  std::ostringstream oss;
  oss << found_tickets.size() << "\n";
  for (const auto &ticket: found_tickets) {
    oss << ticket.format() << '\n';
  }
  return oss.str();
}

// Placed before TrainManager class or as a private nested struct
struct OptimalTransfer {
  QueryTicketInfo leg1_ticket;
  QueryTicketInfo leg2_ticket;
  Time_t total_duration;
  int total_price;
  bool found = false;

  // Default constructor for QueryTicketInfo should exist or be handled
  OptimalTransfer() : total_duration(2000000001), // Initialize with a very large duration
                      total_price(2000000001), // Initialize with a very large price
                      found(false) {
  }

  void update_if_better(const QueryTicketInfo &current_leg1, const QueryTicketInfo &current_leg2,
                        const std::string &sort_pref) {
    // Ensure leg2 departs after or at leg1's arrival (already handled by determine_date for leg2)
    // Ensure not transferring to the same train
    if (current_leg1.train_id_str == current_leg2.train_id_str) {
      return;
    }

    Time_t current_total_duration = current_leg2.at - current_leg1.lt; // Assumes DateTime operator- gives Time_t
    int current_total_price = current_leg1.price + current_leg2.price;

    bool should_update = false;
    if (!found) {
      should_update = true;
    } else {
      if (sort_pref == "time") {
        if (current_total_duration < total_duration) should_update = true;
        else if (current_total_duration == total_duration) {
          if (current_total_price < total_price) should_update = true;
          else if (current_total_price == total_price) {
            if (current_leg1.train_id_str < leg1_ticket.train_id_str) should_update = true;
            else if (current_leg1.train_id_str == leg1_ticket.train_id_str) {
              if (current_leg2.train_id_str < leg2_ticket.train_id_str) should_update = true;
            }
          }
        }
      } else {
        // sort_pref == "cost"
        if (current_total_price < total_price) should_update = true;
        else if (current_total_price == total_price) {
          if (current_total_duration < total_duration) should_update = true;
          else if (current_total_duration == total_duration) {
            if (current_leg1.train_id_str < leg1_ticket.train_id_str) should_update = true;
            else if (current_leg1.train_id_str == leg1_ticket.train_id_str) {
              if (current_leg2.train_id_str < leg2_ticket.train_id_str) should_update = true;
            }
          }
        }
      }
    }

    if (should_update) {
      leg1_ticket = current_leg1;
      leg2_ticket = current_leg2;
      total_duration = current_total_duration;
      total_price = current_total_price;
      found = true;
    }
  }
};


std::string TrainManager::query_transfer(const std::string &from_station_str, const std::string &to_station_str,
                                         const std::string &date_str, const std::string &sort_preference_str) {


  auto res_from = station_name_to_id(from_station_str);
  auto res_to = station_name_to_id(to_station_str);
  if(!res_from||!res_to) {
    return "0\n";
  }
  auto from_id = res_from.value();
  auto to_id = res_to.value();
  TimeUtil::DateTime depart_datetime_from_s(date_str, "00:00");

  OptimalTransfer best_transfer_solution;

  for (int M_id = 0; M_id < next_station_id_val; ++M_id) {
    if (M_id == from_id || M_id == to_id) {
      continue;
    }

    sjtu::vector<TrainManager::BaseTrainInfo> base1 = get_train_in_segment({from_id, M_id});
    sjtu::vector<TrainManager::BaseTrainInfo> base2_temp = get_train_in_segment({M_id, to_id});
    determine_date(base1, depart_datetime_from_s);
    filter_valid_date(base1);
    sjtu::vector<QueryTicketInfo> leg1_tickets = process_output(base1);

    for (const auto & ticket1 : leg1_tickets) {
      TimeUtil::DateTime earliest_depart_time_leg2_from_M = ticket1.at;
      sjtu::vector<TrainManager::BaseTrainInfo> base2 = base2_temp;
      determine_date(base2, earliest_depart_time_leg2_from_M);
      filter_best_date(base2);
      sjtu::vector<QueryTicketInfo> leg2_tickets = process_output(base2);
      for (const auto & ticket2 : leg2_tickets) {
        best_transfer_solution.update_if_better(ticket1, ticket2, sort_preference_str);
      }
    }
  }

  if (best_transfer_solution.found) {
    std::ostringstream oss;
    oss << best_transfer_solution.leg1_ticket.format() << '\n';
    oss << best_transfer_solution.leg2_ticket.format() << '\n';
    return oss.str();
  } else {
    return "0\n";
  }
}

// In train.cpp

std::string TrainManager::buy_ticket(
  OrderManager &order_manager,
  int command_ts,
  const std::string &username_str,
  const std::string &train_id_str,
  const std::string &date_str, // date of departure from from_station_str
  const std::string &num_tickets_str,
  const std::string &from_station_str,
  const std::string &to_station_str,
  const std::string &queue_preference_str
) {
  TrainID_t train_id_key(train_id_str.c_str());
  TimeUtil::DateTime depart_date(date_str);
  int num_tickets = std::stoi(num_tickets_str);
  bool prefer_queue = (queue_preference_str == "true");

  auto res_from = station_name_to_id(from_station_str);
  auto res_to = station_name_to_id(to_station_str);
  if(!res_from||!res_to) {
    return "-1";
  }
  auto from_id = res_from.value();
  auto to_id = res_to.value();

  UsernameKey user_key(username_str);

  auto train_data_opt = train_data_map_.find(train_id_key);
  if (!train_data_opt) return "-1";
  TrainData &train = train_data_opt.value();
  if (!train.release) return "-1";

  Station_idx_t from_idx = -1, to_idx = -1;
  for (Station_idx_t i = 0; i < train.station_num; ++i) {
    if (train.stations[i] == from_id) from_idx = i;
    if (train.stations[i] == to_id) to_idx = i;
  }
  if (from_idx == (Station_idx_t) -1 || to_idx == (Station_idx_t) -1 || from_idx >= to_idx) return "-1";
  TimeUtil::DateTime original_date = train.get_original_date(from_idx, depart_date);

  if (!original_date.isValid() || !train.verify_date(original_date)) {
    return "-1";
  }

  Segment_t journey_segment = {train.train_hash, {from_idx, to_idx}};
  int available_seats = query_seat(train, journey_segment, original_date);

  int ticket_price = train.price_between(from_idx, to_idx);
  TimeUtil::DateTime actual_leave_time = (original_date) + train.get_leave_time(from_idx);
  TimeUtil::DateTime actual_arrive_time = (original_date) + train.get_arrive_time(to_idx);


  if (available_seats >= num_tickets) {
    reduce_seat(train, journey_segment, original_date, num_tickets);
    int total_price = ticket_price * num_tickets;

    Order order(command_ts, OrderStatus::SUCCESS, train_id_key,
                from_id, to_id,
                actual_leave_time, actual_arrive_time,
                ticket_price, num_tickets,
                train.train_hash, original_date, from_idx, to_idx);
    order_manager.record_order(user_key, order);
    return std::to_string(total_price);
  } else {
    if (prefer_queue) {
      if(num_tickets>train.seat_num) {
        return "-1";
      }
      Order order(command_ts, OrderStatus::PENDING, train_id_key,
                  from_id, to_id,
                  actual_leave_time, actual_arrive_time,
                  ticket_price, num_tickets,
                  train.train_hash, original_date, from_idx, to_idx);
      order_manager.record_order(user_key, order);
      return "queue";
    } else {
      return "-1";
    }
  }
}

void TrainManager::process_refunded_tickets(
  OrderManager &order_manager,
  const std::string &train_id_str,
  const std::string &date_depart_from_f_str,
  const std::string &from_station_str,
  const std::string &to_station_str,
  const std::string &num_tickets_str
) {
  TrainID_t train_id_key(train_id_str.c_str());
  int num_refunded_tickets = std::stoi(num_tickets_str);
  if (num_refunded_tickets <= 0) return;

  auto train_data_opt = train_data_map_.find(train_id_key);
  if (!train_data_opt) return;
  TrainData &train = train_data_opt.value();

  auto res_from = station_name_to_id(from_station_str);
  auto res_to = station_name_to_id(to_station_str);
  if(!res_from||!res_to) {
    throw std::runtime_error("Station name not found");
  }
  auto from_id = res_from.value();
  auto to_id = res_to.value();

  Station_idx_t refunded_from_idx = -1, refunded_to_idx = -1;
  for (Station_idx_t i = 0; i < train.station_num; ++i) {
    if (train.stations[i] == from_id) refunded_from_idx = i;
    if (train.stations[i] == to_id) refunded_to_idx = i;
  }
  if (refunded_from_idx == (Station_idx_t) -1 || refunded_to_idx == (Station_idx_t) -1 || refunded_from_idx >=
      refunded_to_idx)
    return;

  TimeUtil::DateTime depart_date(date_depart_from_f_str); // This is the date (mm-dd) at 00:00
  if (!depart_date.isValid()) return;

  TimeUtil::DateTime original_date = train.get_original_date(refunded_from_idx, depart_date);

  if (!original_date.isValid() || !train.verify_date(original_date)) return;

  Segment_t refunded_segment = {train.train_hash, {refunded_from_idx, refunded_to_idx}};

  add_seat(train, refunded_segment, original_date, num_refunded_tickets);

  WaitlistKey wk_key = {train.train_hash, original_date};
  sjtu::vector<WaitlistEntry> waitlist_entries = order_manager.get_wait_list(wk_key);

  RFlowey::quick_sort(waitlist_entries.begin(), waitlist_entries.end(), WaitlistEntry::sortByTimestamp);

  for (const auto &wle: waitlist_entries) {
    OrderKey pending_order_key = {wle.user_hash, wle.command_ts};

    Segment_t pending_journey_segment = {
      wk_key.first, {wle.start_idx, wle.end_idx}
    };

    int seats_needed_for_pending = wle.num_tickets_needed;
    int current_seats_for_pending_journey = query_seat(train, pending_journey_segment, wk_key.second);

    if (current_seats_for_pending_journey >= seats_needed_for_pending) {
      reduce_seat(train, pending_journey_segment, wk_key.second, seats_needed_for_pending);
      order_manager.update_order_status(pending_order_key, OrderStatus::SUCCESS);
      order_manager.remove_from_waitlist(wk_key, wle);
    }
  }
}

void TrainManager::handle_exit() {

  std::ofstream ofs_vec(db_path_prefix + "_station_id_name.dat", std::ios::trunc | std::ios::binary);
  if (ofs_vec.is_open()) {
    ofs_vec.write(reinterpret_cast<const char*>(&next_station_id_val), sizeof(next_station_id_val));
    size_t vec_size = station_id_to_name_vec.size();
    ofs_vec.write(reinterpret_cast<const char*>(&vec_size), sizeof(vec_size));
    for (const auto& name : station_id_to_name_vec) {
      size_t name_len = name.length();
      ofs_vec.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
      ofs_vec.write(name.c_str(), name_len);
    }
    ofs_vec.close();
  } else {
    std::cerr << "Failed to save station ID mapping for TrainManager." << std::endl;
  }
}

void TrainManager::load_id_name_mapping() {
  station_name_to_id_map.clear();
  station_id_to_name_vec.clear();
  next_station_id_val = 0;

  std::ifstream ifs_vec(db_path_prefix + "_station_id_name.dat", std::ios::binary);
  if (ifs_vec.is_open()) {
    ifs_vec.read(reinterpret_cast<char*>(&next_station_id_val), sizeof(next_station_id_val));
    if (ifs_vec.fail()) { ifs_vec.close(); return; }

    size_t vec_size;
    ifs_vec.read(reinterpret_cast<char*>(&vec_size), sizeof(vec_size));
    if (ifs_vec.fail()) { ifs_vec.close(); return; }

    station_id_to_name_vec.resize(vec_size);
    for (size_t i = 0; i < vec_size; ++i) {
      size_t name_len;
      ifs_vec.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
      if (ifs_vec.fail()) { station_id_to_name_vec.clear(); next_station_id_val=0; ifs_vec.close(); return; }

      std::string name(name_len, '\0');
      ifs_vec.read(&name[0], name_len);
      if (ifs_vec.fail()) { station_id_to_name_vec.clear(); next_station_id_val=0; ifs_vec.close(); return; }

      station_id_to_name_vec[i] = name;
      station_name_to_id_map[RFlowey::hash(name)] = i;
    }
    ifs_vec.close();
  }
}