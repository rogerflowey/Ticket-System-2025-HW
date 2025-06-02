#ifndef ORDER_MANAGER_HPP
#define ORDER_MANAGER_HPP

#include "common.h"
#include "user.h"
#include "train.h"
#include <fstream>

enum class OrderStatus : char {
    SUCCESS = 'S',
    PENDING = 'P',
    REFUNDED = 'R'
};

struct OrderRecord {
    int orderID;
    char username[USERNAME_LEN];
    char trainID[TRAIN_ID_LEN];
    int departure_from_f_station_date_day;
    char from_station_name[STATION_NAME_LEN];
    char to_station_name[STATION_NAME_LEN];
    int from_station_route_idx;
    int to_station_route_idx;
    int num_tickets;
    int total_price;
    int order_timestamp;
    OrderStatus status;

    OrderRecord();
};

struct UserOrderKey {
    FixedString<USERNAME_LEN> username;
    int inverted_timestamp;

    bool operator<(const UserOrderKey& other) const;
    bool operator==(const UserOrderKey& other) const;
};

struct PendingOrderQueueKey {
    int order_timestamp;
    int orderID;

    bool operator<(const PendingOrderQueueKey& other) const;
    bool operator==(const PendingOrderQueueKey& other) const;
};

class OrderManager {
private:
    std::fstream orders_file;
    std::fstream system_counters_file;

    const std::string orders_filename = "orders.dat";
    const std::string counters_filename = "system_counters.dat";
    const std::string user_order_idx_filename = "orders_user_timestamp_idx.bpt";
    const std::string pending_idx_filename = "pending_orders_queue.bpt";

    void* orders_user_timestamp_idx_ptr;
    void* pending_orders_queue_idx_ptr;

    int next_order_id_counter;

    UserManager* user_manager_ptr;
    TrainManager* train_manager_ptr;

    void loadNextOrderID();
    void saveNextOrderID();
    int getNextOrderID();

    bool readOrderRecord(long offset, OrderRecord& record_out);
    bool writeOrderRecord(long offset, const OrderRecord& record_in);
    long appendOrderRecord(OrderRecord& record_in);

    void attemptToFulfillPendingOrders();

public:
    OrderManager();
    ~OrderManager();

    void init(UserManager* um, TrainManager* tm);
    void clean();

    int buyTicket(const std::string& username_str,
                  const std::string& trainID_str,
                  int departure_date_day_at_from_station,
                  int num_tickets_to_buy,
                  const std::string& from_station_str,
                  const std::string& to_station_str,
                  bool allow_queueing,
                  int command_timestamp,
                  std::string& result_msg_out);

    int queryOrder(const std::string& username_str);

    int refundTicket(const std::string& username_str, int order_index_in_list);
};

#endif // ORDER_MANAGER_HPP