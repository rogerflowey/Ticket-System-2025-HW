#ifndef TRAIN_MANAGER_HPP
#define TRAIN_MANAGER_HPP

#include "common.h"
#include "utils/train_data.h"
#include <fstream>



struct DailySeatKey {
    int trainID_numeric_hash;
    int segment_actual_date_day;
    int segment_start_station_idx;

    bool operator<(const DailySeatKey& other) const;
    bool operator==(const DailySeatKey& other) const;
};

struct StationTrainListPointer {
    long offset_in_list_file;
    int count_of_trains;
};

class TrainManager {
private:
    std::fstream trains_master_file;
    std::fstream trains_route_blob_file;
    std::fstream station_train_list_file;

    const std::string master_filename = "trains_master.dat";
    const std::string route_blob_filename = "trains_route_blob.dat";
    const std::string station_list_filename = "station_train_list.dat";
    const std::string trainid_idx_filename = "trains_trainID_idx.bpt";
    const std::string station_idx_filename = "station_to_trains_idx.bpt";
    const std::string daily_seats_idx_filename = "daily_seats.bpt";

    void* trains_trainID_idx_ptr;
    void* station_to_trains_idx_ptr;
    void* daily_seats_idx_ptr;

    bool readTrainMasterRecord(long offset, TrainMasterRecord& record_out);
    bool writeTrainMasterRecord(long offset, const TrainMasterRecord& record_in);
    long appendTrainMasterRecord(const TrainMasterRecord& record_in);

    bool storeRouteBlob(int stationNum,
                        const char station_names[][STATION_NAME_LEN],
                        const int prices_to_next[],
                        const int travel_times_to_next[],
                        const int stopover_times[],
                        long& blob_offset_out, int& blob_length_out);

    bool parseRouteBlob(long offset, int length,
                        int expected_station_num,
                        char out_station_names[][STATION_NAME_LEN],
                        int out_prices_to_next[],
                        int out_travel_times_to_next[],
                        int out_stopover_times[]);

    void addTrainToStationIndex(const FixedString<STATION_NAME_LEN>& station_name, const FixedString<TRAIN_ID_LEN>& trainID);
    void removeTrainFromStationIndex(const FixedString<STATION_NAME_LEN>& station_name, const FixedString<TRAIN_ID_LEN>& trainID);

    int getTrainIDHash(const FixedString<TRAIN_ID_LEN>& trainID);

public:
    TrainManager();
    ~TrainManager();

    void init();
    void clean();

    int addTrain(const std::string& trainID_str,
                 int stationNum,
                 const char station_names_arr[][STATION_NAME_LEN],
                 const int prices_arr[],
                 int startTime_minutes_in_day,
                 const int travel_times_arr[],
                 const int stopover_times_arr[],
                 int saleDateStart_day_idx,
                 int saleDateEnd_day_idx,
                 char type,
                 int totalSeatNum);

    int releaseTrain(const std::string& trainID_str);
    int deleteTrain(const std::string& trainID_str);

    int queryTrain(const std::string& trainID_str, int query_date_day_idx);


    int getMinSeatsForJourneySegments(const FixedString<TRAIN_ID_LEN>& trainID,
                                      int origin_departure_date_day_idx,
                                      int from_station_route_idx,
                                      int to_station_route_idx);

    bool updateSeatsForJourneySegments(const FixedString<TRAIN_ID_LEN>& trainID,
                                       int origin_departure_date_day_idx,
                                       int from_station_route_idx,
                                       int to_station_route_idx,
                                       int num_tickets_delta);

    bool findTrainMaster(const std::string& trainID_str, TrainMasterRecord& record_out, long& offset_out);

    // Modified to output a Train object
    bool loadTrainObject(const FixedString<TRAIN_ID_LEN>& trainID_fs, Train& train_obj_out);

    int getTrainsAtStation(const FixedString<STATION_NAME_LEN>& station_name,
                           FixedString<TRAIN_ID_LEN> out_trainIDs_buffer[],
                           int buffer_max_size);
};

#endif // TRAIN_MANAGER_HPP
