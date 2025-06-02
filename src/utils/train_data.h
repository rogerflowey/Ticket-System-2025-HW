#ifndef TRAIN_HPP
#define TRAIN_HPP

#include "common.h"
#include "time.h"
#include <string>

struct TrainMasterRecord {
  char trainID[TRAIN_ID_LEN];
  int stationNum;
  int totalSeatNum;
  int startTime_minutes;
  int saleDateStart_day;
  int saleDateEnd_day;
  char type;
  bool is_released;
  long route_blob_offset;
  int route_blob_length;

  TrainMasterRecord();
};

class Train {
public:
  Train() : station_count_(0) {} // Default constructor
  Train(const TrainMasterRecord& master_data,
        const char station_names_on_route[][STATION_NAME_LEN],
        const int prices_to_next_station[],
        const int travel_times_to_next_station[],
        const int stopover_times_at_station[]);

  FixedString<TRAIN_ID_LEN> getTrainID() const;
  char getType() const;
  int getTotalSeatNum() const;
  bool isReleased() const;
  int getStationCount() const;
  int getSaleDateStartDay() const;
  int getSaleDateEndDay() const;
  int getOriginStartTimeMinutes() const;

  int getStationIndex(const FixedString<STATION_NAME_LEN>& station_name) const;
  FixedString<STATION_NAME_LEN> getStationName(int station_idx) const;

  int getPriceToStation(int station_idx) const;
  int getPriceBetweenStations(int from_station_idx, int to_station_idx) const;

  int getArrivalTimeAtStation(int station_idx, int origin_departure_day_idx) const;
  int getDepartureTimeFromStation(int station_idx, int origin_departure_day_idx) const;
  int getTravelDurationMinutes(int from_station_idx, int to_station_idx) const;

  int calculateOriginDepartureDayIndex(int f_station_idx,
                                       int departure_day_at_f_station_idx) const;

private:
  TrainMasterRecord master_data_;

  FixedString<STATION_NAME_LEN> station_names_[MAX_STATIONS_ON_ROUTE];
  int station_count_;

  int cumulative_prices_[MAX_STATIONS_ON_ROUTE];
  int scheduled_arrival_offset_minutes_[MAX_STATIONS_ON_ROUTE];
  int scheduled_departure_offset_minutes_[MAX_STATIONS_ON_ROUTE];

  void precomputeSchedulesAndPrices(const int prices_to_next[],
                                    const int travel_times_to_next[],
                                    const int stopover_times[]);
};

#endif // TRAIN_HPP