#pragma once

#include <map.hpp>
#include <string>
#include <optional>
#include <utility>

#include "utils.h"
#include "database.h"
#include "datetime.h"
#include "common.h"
#include "my_fileconfig.h"
#include "string_utils.h"

class OrderManager;


inline sjtu::map<hash_t, int> station_name_to_id_map;
inline sjtu::vector<std::string> station_id_to_name_vec;
inline int next_station_id_val = 0;

inline std::optional<int> station_name_to_id(const std::string& station_name) {
  auto it = station_name_to_id_map.find(RFlowey::hash(station_name));
  if(it!=station_name_to_id_map.end()) {
    return it->second;
  }
  return std::nullopt;
}

using TrainID_t = RFlowey::string<21>;
using Station_idx_t = unsigned short;
using Segment_t = RFlowey::pair<hash_t, RFlowey::pair<Station_idx_t, Station_idx_t> >;

struct StationPairHasher {
  size_t operator()(const RFlowey::pair<int, int>& p) const {
    return p.first*(1<<16)+p.second;
  }
};

struct TrainData {
  TrainID_t train_id;
  hash_t train_hash;
  int station_num;
  int stations[25];
  size_t seat_num;
  int prices[25];
  Time_t start_time;
  int travel_times[25];
  int stopover_times[25];
  TimeUtil::DateTime sale_start;
  TimeUtil::DateTime sale_end;
  char type;
  bool release = false;


  int price_between(int from_idx, int to_idx) {
    if (from_idx < 0 || to_idx >= station_num || from_idx >= to_idx) {
      return -1;
    };

    int total_price = 0;
    for (int i = from_idx; i < to_idx; ++i) {
      total_price += prices[i];
    }
    return total_price;
  }


  Time_t time_between(int from_idx, int to_idx) {
    if (from_idx < 0 || to_idx >= station_num || from_idx >= to_idx) {
      return -1;
    }
    Time_t total_travel_time = 0;

    for (int i = from_idx; i < to_idx; ++i) {
      total_travel_time += travel_times[i];
    }

    if (station_num > 2) {
      for (int i = from_idx; i < to_idx - 1; ++i) {
        if (i < station_num - 2) {
          total_travel_time += stopover_times[i];
        }
      }
    };
    return total_travel_time;
  }

  TimeUtil::DateTime get_original_date(int station_idx, TimeUtil::DateTime depart_time) {
    Time_t minute_from_origin = get_leave_time(station_idx);
    TimeUtil::DateTime min_origin_depart = depart_time - minute_from_origin;
    return  min_origin_depart.roundUpToDate();
  }

  //Get the arrive time since the original **Date**
  Time_t get_arrive_time(Station_idx_t target_idx) const {
    if (target_idx >= station_num) {
      return {};
    }
    Time_t current_dt=start_time;
    for (Station_idx_t i = 0; i < target_idx; ++i) {
      current_dt = current_dt + travel_times[i];
      if (i < target_idx - 1) {
        if (station_num > 2 && i < station_num - 2) {
          current_dt = current_dt + stopover_times[i];
        }
      }
    }
    return current_dt;
  }

  //Get the depart time since the original **Date**
  Time_t get_leave_time(Station_idx_t target_idx) const {
    if (target_idx >= station_num || target_idx == station_num - 1) {
      return {};
    }
    Time_t arrival_at_target = get_arrive_time(target_idx);
    if (target_idx == 0) {
      return arrival_at_target;
    }
    Time_t leave_dt = arrival_at_target;
    if (station_num > 2) {
      leave_dt = leave_dt + stopover_times[target_idx - 1];
    }
    return leave_dt;
  }

  bool verify_date(TimeUtil::DateTime original_date) {
    return (sale_start<=original_date)&&(sale_end>=original_date);
  }
  std::optional<TimeUtil::DateTime> find_earliest(TimeUtil::DateTime original_date) {
    if((sale_start<=original_date)&&(sale_end>=original_date)){
      return original_date;
    }
    if(original_date<sale_start) {
      return sale_start;
    }
    return std::nullopt;
  }

  bool parse_arguments(
    const std::string &i_trainID_str,
    const std::string &n_stationNum_str,
    const std::string &m_seatNum_str,
    const std::string &s_stations_str,
    const std::string &p_prices_str,
    const std::string &x_startTime_str,
    const std::string &t_travelTimes_str,
    const std::string &o_stopoverTimes_str,
    const std::string &d_saleDate_str,
    const std::string &y_type_str
  ) {
    train_id = TrainID_t(i_trainID_str.c_str());
    train_hash = RFlowey::hasher<21>{}(i_trainID_str);

    station_num = static_cast<size_t>(std::stoull(n_stationNum_str));

    seat_num = static_cast<size_t>(std::stoull(m_seatNum_str));

    sjtu::vector<std::string> station_names = split(s_stations_str, '|');
    for (size_t i = 0; i < station_num; ++i) {
      const std::string& current_station_name = station_names[i];
      auto res = station_name_to_id(current_station_name);
      if (!res) {
        int new_id = next_station_id_val++;
        station_name_to_id_map.insert({RFlowey::hash(current_station_name), new_id});
        if (new_id >= station_id_to_name_vec.size()) {
          station_id_to_name_vec.resize(new_id + 1);
        }
        station_id_to_name_vec[new_id] = current_station_name;
        stations[i] = new_id;
      } else {
        stations[i] = res.value();
      }
    }

    size_t num_prices = station_num - 1;
    sjtu::vector<std::string> price_strs = split(p_prices_str, '|');
    for (size_t i = 0; i < num_prices; ++i) {
      prices[i] = static_cast<size_t>(std::stoull(price_strs[i]));
    }
    start_time = TimeUtil::minutesInDayFromTimeString(x_startTime_str);
    size_t num_travel_times = station_num - 1;
    sjtu::vector<std::string> travel_time_strs = split(t_travelTimes_str, '|');
    for (size_t i = 0; i < num_travel_times; ++i) {
      travel_times[i] = static_cast<Time_t>(std::stoll(travel_time_strs[i]));
    }

    if (station_num > 2) {
      size_t num_stopovers = station_num - 2;
      sjtu::vector<std::string> stopover_time_strs = split(o_stopoverTimes_str, '|');
      // Assuming stopover_time_strs.size() == num_stopovers and each time string is valid.
      for (size_t i = 0; i < num_stopovers; ++i) {
        stopover_times[i] = static_cast<Time_t>(std::stoll(stopover_time_strs[i]));
      }
    }
    sjtu::vector<std::string> sale_date_parts = split(d_saleDate_str, '|');
    sale_start = TimeUtil::DateTime(sale_date_parts[0], "00:00");
    sale_end = TimeUtil::DateTime(sale_date_parts[1], "23:59");
    type = y_type_str[0];

    return true;
  }
};

struct QueryTicketInfo {
  std::string train_id_str;
  std::string fs_name;
  TimeUtil::DateTime lt;
  std::string ts_name;
  TimeUtil::DateTime at;
  int price;
  int sc;
  Time_t duration;

  QueryTicketInfo() = default;
  QueryTicketInfo(std::string  tid, std::string  fsn, const TimeUtil::DateTime& l_time,
                  std::string  tsn, const TimeUtil::DateTime& a_time, int p, int s_count, Time_t dur)
      : train_id_str(std::move(tid)), fs_name(std::move(fsn)), lt(l_time), ts_name(std::move(tsn)), at(a_time), price(p), sc(s_count), duration(dur) {}

  std::string format() const {
    std::ostringstream oss;
    oss << train_id_str << " "
        << fs_name << " " << lt.getFullString() << " -> "
        << ts_name << " " << at.getFullString() << " "
        << price << " " << sc;
    return oss.str();
  }
  static bool sortByTime(const QueryTicketInfo& a, const QueryTicketInfo& b) {
    if (a.duration != b.duration) return a.duration < b.duration;
    return a.train_id_str < b.train_id_str;
  }
  static bool sortByCost(const QueryTicketInfo& a, const QueryTicketInfo& b) {
    if (a.price != b.price) return a.price < b.price;
    return a.train_id_str < b.train_id_str;
  }
  static bool sortByArriveTime(const QueryTicketInfo& a, const QueryTicketInfo& b) {
    if (a.at != b.at) return a.at < b.at;
    return a.train_id_str < b.train_id_str;
  }
};


class TrainManager {
  HashedSingleMap<TrainID_t, TrainData, RFlowey::hasher<21> > train_data_map_;
  OrderedHashMap<RFlowey::pair<int, int>, Segment_t, StationPairHasher> seg_to_train_;
  SingleMap<RFlowey::pair<TimeUtil::DateTime,RFlowey::pair<hash_t, Station_idx_t>>, int> daily_seat;

  hash_t hashSeg(const std::string &station1, const std::string &station2) {
    return norb::hash::djb2_hash(station1) + norb::hash::djb2_hash(station2);
  }

  int query_seat(const TrainData &train, Segment_t seg, TimeUtil::DateTime date);

  bool reduce_seat(const TrainData &train, Segment_t seg, const TimeUtil::DateTime &date, int num_tickets);

  bool add_seat(const TrainData &train, Segment_t seg, const TimeUtil::DateTime &date, int num_tickets);

  struct BaseTrainInfo {
    TrainData train;
    Segment_t seg;
    TimeUtil::DateTime original_date;
  };


  sjtu::vector<TrainManager::BaseTrainInfo> get_train_in_segment(RFlowey::pair<int, int> station_pair_key);

  void determine_date(sjtu::vector<TrainManager::BaseTrainInfo>& base,TimeUtil::DateTime min_depart_time);
  sjtu::vector<QueryTicketInfo> process_output(sjtu::vector<TrainManager::BaseTrainInfo>& base);
  void filter_valid_date(sjtu::vector<TrainManager::BaseTrainInfo>& base);
  void filter_best_date(sjtu::vector<TrainManager::BaseTrainInfo>& base);

  sjtu::vector<QueryTicketInfo> find_direct(RFlowey::pair<int, int> seg_key, TimeUtil::DateTime depart_date);




public:
  constexpr static std::string db_path_prefix = "train_data";

  TrainManager();

  ~TrainManager() = default;


  /**
   * @brief Adds a new train to the system.
   * Corresponds to the 'add_train' command.
   * @param train_id_str Unique identifier for the train (-i).
   * @param station_num_str Number of stations the train passes (-n).
   * @param seat_num_str Total number of seats on the train (-m).
   * @param stations_str Pipe-separated list of station names (-s).
   * @param prices_str Pipe-separated list of prices between consecutive stations (-p).
   * @param start_time_str Daily departure time from the origin station (hh:mm) (-x).
   * @param travel_times_str Pipe-separated list of travel times (minutes) between consecutive stations (-t).
   * @param stopover_times_str Pipe-separated list of stopover times (minutes) at intermediate stations; "_" if N/A (-o).
   * @param sale_date_str Pipe-separated start and end dates for ticket sales (mm-dd|mm-dd) (-d).
   * @param type_str A single character representing the train type (-y).
   * @return "0" on success, "-1" on failure (e.g., trainID already exists).
   */
  int add_train(
    const std::string &train_id_str,
    const std::string &station_num_str,
    const std::string &seat_num_str,
    const std::string &stations_str,
    const std::string &prices_str,
    const std::string &start_time_str,
    const std::string &travel_times_str,
    const std::string &stopover_times_str,
    const std::string &sale_date_str,
    const std::string &type_str
  );

  /**
   * @brief Deletes a train from the system.
   * Corresponds to the 'delete_train' command. The train must not be released.
   * @param train_id_str The ID of the train to delete (-i).
   * @return "0" on success, "-1" on failure (e.g., train not found or already released).
   */
  int delete_train(const std::string &train_id_str);

  /**
   * @brief Releases a train, making it available for ticket sales and queries.
   * Corresponds to the 'release_train' command. Once released, a train cannot be deleted or modified.
   * @param train_id_str The ID of the train to release (-i).
   * @return "0" on success, "-1" on failure (e.g., train not found or already released).
   */
  int release_train(const std::string &train_id_str);


  /**
   * @brief Queries the details of a specific train on a given date.
   * Corresponds to the 'query_train' command.
   * @param train_id_str The ID of the train to query (-i).
   * @param date_str The date of departure from the train's origin station (mm-dd) (-d).
   * @return A multi-line string with train details (schedule, prices, seat availability) on success,
   *         or "-1" on failure (e.g., train not found, not released, or not running on the specified date).
   */
  std::string query_train(const std::string &train_id_str, const std::string &date_str);

  /**
   * @brief Queries for available tickets between two stations on a specific date.
   * Corresponds to the 'query_ticket' command.
   * @param from_station_str Departure station name (-s).
   * @param to_station_str Arrival station name (-t).
   * @param date_str Date of departure from the 'from_station_str' (mm-dd) (-d).
   * @param sort_preference_str Sorting preference: "time" or "cost" (-p).
   * @return A multi-line string. The first line is the count of found train services.
   *         Subsequent lines list available tickets, sorted as per preference.
   *         Each line: <trainID> <FROM> <LEAVING_TIME> -> <TO> <ARRIVING_TIME> <PRICE> <SEAT>.
   */
  std::string query_ticket(
    const std::string &from_station_str,
    const std::string &to_station_str,
    const std::string &date_str,
    const std::string &sort_preference_str
  );

  /**
   * @brief Queries for transfer options (one transfer) between two stations.
   * Corresponds to the 'query_transfer' command.
   * @param from_station_str Departure station name (-s).
   * @param to_station_str Arrival station name (-t).
   * @param date_str Date of departure from the 'from_station_str' (mm-dd) for the first leg of the journey (-d).
   * @param sort_preference_str Sorting preference for the optimal solution: "time" or "cost" (-p).
   * @return A multi-line string (2 lines, one for each train in the transfer) if an optimal transfer is found.
   *         Returns "0" (as a string, representing count 0) if no suitable transfer route exists.
   */
  std::string query_transfer(
    const std::string &from_station_str,
    const std::string &to_station_str,
    const std::string &date_str,
    const std::string &sort_preference_str
  );

  std::optional<std::pair<Station_idx_t, Station_idx_t>> find_station_indices(
      const TrainData &train, int from_station_id, int to_station_id);

public:
  // ... (other methods)

  std::string buy_ticket(
    OrderManager& order_manager,
    int command_ts,
    const std::string &username_str,
    const std::string &train_id_str,
    const std::string &date_str, // This is departure date from from_station_str
    const std::string &num_tickets_str,
    const std::string &from_station_str,
    const std::string &to_station_str,
    const std::string &queue_preference_str
  );

  void process_refunded_tickets(
    OrderManager& order_manager,
    const std::string &train_id_str,
    const std::string &date_depart_from_f_str, // Date of departure from from_station_str
    const std::string &from_station_str,
    const std::string &to_station_str,
    const std::string &num_tickets_str
  );

  void handle_exit();

  void load_id_name_mapping();
  void clean_data() {
    station_name_to_id_map.clear();
    station_id_to_name_vec.clear();
    next_station_id_val = 0;
    train_data_map_.clear();
    seg_to_train_.clear();
    daily_seat.clear();
  };
private:



};
