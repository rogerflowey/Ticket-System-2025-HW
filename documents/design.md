**Overall System Design: Train Ticket Booking System**

**Guiding Principles:**

*   **File-Based Storage:** All persistent data will be stored in local files.
*   **B+ Tree Indexing:** B+ trees (custom implementation or provided basic one) will be the primary mechanism for efficient data retrieval.
*   **Limited Libraries:** Adherence to the specified C++ headers. No STL containers (except `std::string`) or algorithms.
*   **Efficiency for Frequent Operations:** Prioritize performance for `query_ticket`, `buy_ticket`, `login`, `logout`, `query_profile`.

---

**#1 Data Structures and File Layout**

We'll conceptualize data in "tables," each likely mapping to one or more data files and associated B+ tree index files.

**A. User Management**

1.  **`users.dat` (User Data File)**
    *   Stores fixed-size records for each user.
    *   **Fields per record:**
        *   `username` (char[21])
        *   `password_hash` (char[65] - e.g., for SHA-256 hex string)
        *   `name` (char[16] - 5 Hanzi * 3 bytes/Hanzi + null terminator)
        *   `mailAddr` (char[31])
        *   `privilege` (int)
2.  **`users_username_idx.bpt` (User Index by Username)**
    *   B+ Tree.
    *   **Key:** `username` (string)
    *   **Value:** `record_offset` (long) in `users.dat`.
3.  **`online_users.bpt` (Logged-in Users)**
    *   B+ Tree (used like a set).
    *   **Key:** `username` (string)
    *   **Value:** `1` (or any small constant, presence indicates logged in).

**B. Train Management**

1.  **`trains_master.dat` (Train Master Data File)**
    *   Stores fixed-size part of train data.
    *   **Fields per record:**
        *   `trainID` (char[21])
        *   `stationNum` (int)
        *   `seatNum` (int) - Total seats on this train type.
        *   `startTime_minutes` (int) - Start time (hh:mm) converted to minutes since midnight.
        *   `saleDateStart_day` (int) - e.g., days since 2025-01-01 or 2025-06-01.
        *   `saleDateEnd_day` (int)
        *   `type` (char)
        *   `is_released` (bool)
        *   `route_blob_offset` (long) - Offset into `trains_route_blob.dat`.
        *   `route_blob_length` (int) - Length of the blob for this train.
2.  **`trains_trainID_idx.bpt` (Train Index by trainID)**
    *   B+ Tree.
    *   **Key:** `trainID` (string)
    *   **Value:** `record_offset` (long) in `trains_master.dat`.
3.  **`trains_route_blob.dat` (Train Route Details Blob File)**
    *   Stores variable-length serialized data for each train's route.
    *   **Content for one train (pointed to by `route_blob_offset`/`length`):**
        A single string or binary block containing:
        `station1|station2|...|stationN\nprice1|price2|...\ntravelTime1|travelTime2|...\nstopoverTime1|stopoverTime2|...`
        (Need a clear parsing strategy).
4.  **`station_to_trains_idx.bpt` (Station to Train Index - Inverted Index)**
    *   B+ Tree.
    *   **Key:** `station_name` (string)
    *   **Value:** `offset_in_station_train_list_file` (long), `date range` (int-int).
5.  **`station_train_list.dat` (Train Lists per Station)**
    *   Stores lists of `trainID`s. `station_to_trains_idx.bpt` points into this file.
    *   Each entry is a block of `trainID`s (char[21]).

**C. Daily Seat Availability**

1.  **`daily_seats.bpt` (Daily Seat Availability per Segment)**
    *   B+ Tree.
    *   **Key:** Composite key: `(trainID_numeric_hash, segment_actual_date_day, segment_start_station_idx)`
        *   `trainID_numeric_hash` (int/long): Hash of `trainID` string to make keys more uniform/shorter.
        *   `segment_actual_date_day` (int): The specific date (days since reference) this segment is operational for a particular train run.
        *   `segment_start_station_idx` (int): 0-based index of the first station in the segment *within its train's route*.
    *   **Value:** `available_seats` (int).

**D. Order Management**

1.  **`orders.dat` (Order Data File)**
    *   Stores fixed-size records for each order.
    *   **Fields per record:**
        *   `orderID` (int) - System-wide unique, can be a simple counter stored in a separate small file or derived.
        *   `username` (char[21])
        *   `trainID` (char[21])
        *   `departure_from_station_date_day` (int) - Date of departure *from the -f station*.
        *   `from_station_name` (char[31])
        *   `to_station_name` (char[31])
        *   `from_station_idx` (int) - Index of from_station in train's route.
        *   `to_station_idx` (int) - Index of to_station in train's route.
        *   `num_tickets` (int)
        *   `total_price` (int)
        *   `order_timestamp` (int) - Timestamp of the `buy_ticket` command.
        *   `status` (char) - e.g., 'S' (success), 'P' (pending), 'R' (refunded).
2.  **`orders_user_timestamp_idx.bpt` (User Order Index)**
    *   B+ Tree for `query_order`.
    *   **Key:** Composite key: `(username_numeric_hash, MAX_INT - order_timestamp)`
        *   `username_numeric_hash` (int/long): Hash of `username`.
        *   `MAX_INT - order_timestamp`: To sort by timestamp descending.
    *   **Value:** `record_offset` (long) in `orders.dat` (or `orderID` if `orderID` itself is an index to `orders.dat`).
3.  **`pending_orders_queue.bpt` (Pending Orders Queue)**
    *   B+ Tree for managing the候补队列.
    *   **Key:** `order_timestamp` (int) - To process in FIFO order.
    *   **Value:** `order_record_offset` (long) in `orders.dat` (or `orderID`).

**E. System Utilities**

1.  **`system_counters.dat` (e.g., for next OrderID)**
    *   Small file to store persistent counters if needed.

---

**#2 Operations**

**User Related**

*   **`add_user -c <C> -u <U> -p <P> -n <N> -m <M> -g <G>`**
    1.  **First User Check**: Check if `users_username_idx.bpt` is empty or a special flag file exists.
        *   If first user: `privilege = 10`. Ignore `-c`, `-g`.
    2.  **Non-First User**:
        *   Query `online_users.bpt` for `<C>`. If not logged in, fail (-1).
        *   Fetch `<C>`'s data from `users.dat` (via `users_username_idx.bpt`) to get `C_privilege`.
        *   If `<G> >= C_privilege`, fail (-1).
    3.  Query `users_username_idx.bpt` for `<U>`. If exists, fail (-1).
    4.  Hash password `<P>`.
    5.  Append new user record to `users.dat`.
    6.  Insert `(<U>, new_record_offset)` into `users_username_idx.bpt`.
    7.  Return 0.

*   **`login -u <U> -p <P>`**
    1.  Query `users_username_idx.bpt` for `<U>`. If not found, fail (-1). Get `record_offset`.
    2.  Read user record from `users.dat` at `record_offset`.
    3.  Hash `<P>` and compare with stored `password_hash`. If mismatch, fail (-1).
    4.  Query `online_users.bpt` for `<U>`. If exists, fail (-1) (already logged in).
    5.  Insert `(<U>, 1)` into `online_users.bpt`.
    6.  Return 0.

*   **`logout -u <U>`**
    1.  Query `online_users.bpt` for `<U>`. If not found, fail (-1).
    2.  Remove `<U>` from `online_users.bpt`.
    3.  Return 0.

*   **`query_profile -c <C> -u <U>`**
    1.  Query `online_users.bpt` for `<C>`. If not logged in, fail (-1).
    2.  Fetch `<C>`'s data for `C_privilege`. Fetch `<U>`'s data for `U_privilege` and other details (from `users.dat` via `users_username_idx.bpt`). If `<U>` not found, fail (-1).
    3.  If `C != U` AND `C_privilege <= U_privilege`, fail (-1).
    4.  Return `<U> <name_U> <mailAddr_U> <privilege_U>`.

*   **`modify_profile -c <C> -u <U> (-p) (-n) (-m) (-g)`**
    1.  Query `online_users.bpt` for `<C>`. If not logged in, fail (-1).
    2.  Fetch `<C>`'s data for `C_privilege`. Fetch `<U>`'s data for `U_privilege` (from `users.dat` via `users_username_idx.bpt`). If `<U>` not found, fail (-1).
    3.  Permission check:
        *   If `C != U` AND `C_privilege <= U_privilege`, fail (-1).
        *   If `-g <new_privilege>` is provided AND `<new_privilege> >= C_privilege` (and `C != U`), fail (-1). (Note: if `C==U`, they can't elevate their own privilege beyond what it was, unless they are admin modifying someone with lower priv). The rule is "`-g`需低于`-c`的权限".
    4.  Read `<U>`'s record from `users.dat`.
    5.  Update fields if provided: hash new password if `-p`, update name if `-n`, etc.
    6.  Write modified record back to `users.dat` at the same offset.
    7.  Return modified profile string.

**Train Related**

*   **Helper Function: `parse_train_route_blob(blob_data)`**: Takes raw blob, returns structured station names, prices, travel times, stopover times.
*   **Helper Function: `serialize_train_route_data(...)`**: Takes structured route data, returns a blob for storage.
*   **Helper Function: `calculate_time_price_arrays(train_master_data, parsed_route_data)`**: Calculates cumulative times and prices from origin for each station.

*   **`add_train -i <ID> ...`**
    1.  Query `trains_trainID_idx.bpt` for `<ID>`. If exists, fail (-1).
    2.  Parse `-s`, `-p`, `-t`, `-o` strings. Validate `stationNum`.
    3.  Convert `-x` (startTime) to `startTime_minutes`. Convert `-d` (saleDate) to `saleDateStart_day`, `saleDateEnd_day`.
    4.  Serialize station names, prices, travel times, stopover times into a route blob.
    5.  Append route blob to `trains_route_blob.dat`. Get `route_blob_offset`, `route_blob_length`.
    6.  Create `train_master_record` with all data. Append to `trains_master.dat`. Get `master_record_offset`.
    7.  Insert `(<ID>, master_record_offset)` into `trains_trainID_idx.bpt`.
    8.  **Update Station-to-Train Index**: For each station name in `-s`:
        a.  Query `station_to_trains_idx.bpt` for `station_name`.
        b.  If not found: create new entry in `station_train_list.dat` with `<ID>`, update `station_to_trains_idx.bpt`.
        c.  If found: retrieve list from `station_train_list.dat`, add `<ID>` (may involve reallocating/updating list file and index), update `station_to_trains_idx.bpt`.
    9.  Return 0.

*   **`delete_train -i <ID>`**
    1.  Query `trains_trainID_idx.bpt` for `<ID>`. If not found, fail (-1). Get `master_record_offset`.
    2.  Read train master record from `trains_master.dat`. If `is_released == true`, fail (-1).
    3.  Mark record in `trains_master.dat` as deleted (or implement actual deletion and compaction if time permits; marking is safer/simpler).
    4.  Remove `<ID>` from `trains_trainID_idx.bpt`.
    5.  (Harder part) Remove `<ID>` from all lists in `station_train_list.dat` it belongs to, and update `station_to_trains_idx.bpt` if lists become empty or change location. This can be complex. A simpler approach might be to just leave stale entries in `station_train_list.dat` and filter them out during queries if the train is marked deleted.
    6.  Return 0.

*   **`release_train -i <ID>`**
    1.  Query `trains_trainID_idx.bpt` for `<ID>`. If not found, fail (-1). Get `master_record_offset`.
    2.  Read train master record from `trains_master.dat`. If `is_released == true`, fail (-1).
    3.  Update `is_released = true` in the record in `trains_master.dat`.
    4.  **Populate `daily_seats.bpt`**:
        a.  Get `seatNum`, `saleDateStart_day`, `saleDateEnd_day`.
        b.  Parse route data from `trains_route_blob.dat`.
        c.  Calculate `trainID_numeric_hash`.
        d.  For `current_day` from `saleDateStart_day` to `saleDateEnd_day` (this is origin departure day):
        i.  Initialize `time_elapsed_from_origin_start = 0`.
        ii. For `seg_idx` from `0` to `stationNum - 2` (for each segment):
        1.  Calculate `segment_actual_date_day` based on `current_day` (origin departure) and `time_elapsed_from_origin_start`.
        2.  Key: `(trainID_numeric_hash, segment_actual_date_day, seg_idx)`.
        3.  Insert `(Key, seatNum)` into `daily_seats.bpt`.
        4.  Update `time_elapsed_from_origin_start` by adding `travelTimes[seg_idx]` and (if not last segment before destination and `seg_idx < stationNum-2`) `stopoverTimes[seg_idx]`.
    5.  Return 0.

*   **`query_train -i <ID> -d <Date>`**
    1.  Query `trains_trainID_idx.bpt` for `<ID>`. If not found, fail (-1). Get `master_record_offset`.
    2.  Read train master record. Parse route data. Convert `<Date>` to `query_origin_date_day`.
    3.  If `query_origin_date_day` not in train's `saleDate` range, fail (-1).
    4.  Output first line: `<trainID> <type>`.
    5.  Calculate cumulative times from origin departure (for arrival/departure at each station) and cumulative prices.
    6.  `current_time_abs = query_origin_date_day` (as absolute time) + `startTime_minutes`.
    7.  For `station_idx` from `0` to `stationNum - 1`:
        a.  `station_name = stations[station_idx]`.
        b.  `arrival_time_str`, `leaving_time_str`:
        *   Start station: arrival "xx-xx xx:xx". Leaving is `current_time_abs`.
        *   Intermediate: arrival is `current_time_abs`. Leaving is `current_time_abs + stopoverTimes[station_idx-1]`.
        *   End station: arrival is `current_time_abs`. Leaving "xx-xx xx:xx".
        c.  `price_str`: Cumulative price from origin to this station. (0 for start).
        d.  `seat_str`:
        *   If end station, "x".
        *   Else, if `!is_released`, then `seatNum`.
        *   Else (released):
        i.  Calculate `segment_actual_date_day` for the segment starting at `station_idx` based on `query_origin_date_day`.
        ii. Key: `(trainID_numeric_hash, segment_actual_date_day, station_idx)`.
        iii.Query `daily_seats.bpt` for `Key`. Get `available_seats`.
        iv. `seat_str = available_seats`.
        e.  Output line: `<station_name> <arrival_time_str> -> <leaving_time_str> <price_str> <seat_str>`.
        f.  Update `current_time_abs` by adding `travelTimes[station_idx]` and (if applicable) `stopoverTimes[station_idx]`.
    8.  Return.

**Ticket Booking & Querying**

*   **Helper Function: `calculate_origin_departure_date(trainID, from_station_idx, departure_date_from_station, departure_time_at_station)`**:
    *   Loads train route, calculates time from origin to `from_station_idx` departure.
    *   Subtracts this from `departure_date_from_station` + `departure_time_at_station` to get origin departure datetime. Returns date part.
*   **Helper Function: `get_segment_seat_availability(trainID_hash, origin_dep_date, from_idx, to_idx, num_tickets_needed)`**:
    *   Iterates segments from `from_idx` to `to_idx-1`.
    *   For each segment, calculates its `actual_segment_date`.
    *   Queries `daily_seats.bpt`.
    *   Returns `min_available_seats` across these segments. If any segment has `< num_tickets_needed`, can return a special value.

*   **`query_ticket -s <S> -t <T> -d <D_S> (-p <sort_pref>)`**
    1.  Convert `<D_S>` to `departure_S_date_day`.
    2.  Query `station_to_trains_idx.bpt` for `<S>` -> `list_S_trains`.
    3.  Query `station_to_trains_idx.bpt` for `<T>` -> `list_T_trains`.
    4.  Intersect `list_S_trains` and `list_T_trains` -> `common_trainIDs`.
    5.  Initialize empty `result_list`.
    6.  For each `trainID` in `common_trainIDs`:
        a.  Fetch train master data. If not `is_released`, skip.
        b.  Parse route data. Find `idx_S` and `idx_T`. If `idx_S >= idx_T`, skip.
        c.  Calculate `departure_time_S_minutes` (from train's schedule at station `S`).
        d.  `origin_dep_date = calculate_origin_departure_date(trainID, idx_S, departure_S_date_day, departure_time_S_minutes)`.
        e.  If `origin_dep_date` not in train's `saleDate` range, skip.
        f.  `min_seats = get_segment_seat_availability(trainID_hash, origin_dep_date, idx_S, idx_T, 1)`.
        g.  If `min_seats > 0`:
        i.  Calculate price from `S` to `T`.
        ii. Calculate absolute departure datetime from `S` and arrival datetime at `T`.
        iii.Add `(trainID, S, dep_dt_S, T, arr_dt_T, price, min_seats)` to `result_list`.
    7.  Sort `result_list` based on `-p` (time or cost, then `trainID`).
    8.  Print count, then each result.

*   **`buy_ticket -u <U> -i <ID> -d <D_F> -n <N> -f <F> -t <T> (-q <Q>)`**
    1.  Verify `<U>` logged in (`online_users.bpt`). Fail (-1) if not.
    2.  Fetch train `<ID>` master data. If not found or not `is_released`, fail (-1).
    3.  Parse route data. Find `idx_F`, `idx_T`. If `idx_F >= idx_T` or not found, fail (-1).
    4.  Convert `<D_F>` to `departure_F_date_day`.
    5.  Calculate `departure_time_F_minutes` (from train's schedule at station `F`).
    6.  `origin_dep_date = calculate_origin_departure_date(ID, idx_F, departure_F_date_day, departure_time_F_minutes)`.
    7.  If `origin_dep_date` not in train's `saleDate` range, fail (-1).
    8.  **Atomically Check & Update Seats (Conceptual - two phases):**
        *   **Phase 1: Check Seats**
            *   `can_buy = true`. `segments_to_update = []`.
            *   For `seg_idx` from `idx_F` to `idx_T - 1`:
                i.  Calculate `actual_segment_date_day` for this segment based on `origin_dep_date`.
                ii. Key: `(trainID_hash, actual_segment_date_day, seg_idx)`.
                iii.Query `daily_seats.bpt` for `Key` -> `available_seats`.
                iv. If `available_seats < N`, `can_buy = false`, break.
                v.  Add `(Key, available_seats)` to `segments_to_update`.
        *   **Phase 2: Process based on check**
            *   If `can_buy == true`:
                i.  For each `(Key, old_seats)` in `segments_to_update`: Update `daily_seats.bpt` for `Key` to `old_seats - N`.
                ii. Calculate total price. Create order record (status 'S'). Append to `orders.dat`. Update `orders_user_timestamp_idx.bpt`.
                iii.Return total price.
            *   Else (`can_buy == false`):
                i.  If `-q true`: Create order record (status 'P'). Append to `orders.dat`. Update `orders_user_timestamp_idx.bpt`. Insert into `pending_orders_queue.bpt`. Return "queue".
                ii. Else: Return -1.

*   **`query_order -u <U>`**
    1.  Verify `<U>` logged in. Fail (-1) if not.
    2.  `username_hash = hash(U)`.
    3.  Perform a range scan on `orders_user_timestamp_idx.bpt` for keys starting with `(username_hash, ...)`.
    4.  For each `record_offset` found, read order from `orders.dat`.
    5.  Collect all orders for user `<U>`. (They are already sorted new to old due to index key).
    6.  Print count, then each order details.

*   **`refund_ticket -u <U> (-n <idx_to_refund>)`**
    1.  Verify `<U>` logged in.
    2.  Query orders for `<U>` as in `query_order` to get an ordered list of their `order_offsets` or `orderIDs`.
    3.  If `<idx_to_refund>` is out of bounds, fail (-1). Get the target `order_offset`.
    4.  Read target order record from `orders.dat`.
    5.  If status is 'refunded', fail (-1).
    6.  If status is 'success':
        a.  For `seg_idx` from `order.from_station_idx` to `order.to_station_idx - 1`:
        i.  Calculate `origin_dep_date` for this order's train journey.
        ii. Calculate `actual_segment_date_day` for this segment.
        iii.Key: `(trainID_hash, actual_segment_date_day, seg_idx)`.
        iv. Read `current_seats` from `daily_seats.bpt`. Update to `current_seats + order.num_tickets`.
        b.  Update order status to 'R' in `orders.dat`.
        c.  **Process Pending Queue**:
        Iterate through `pending_orders_queue.bpt` (oldest first by `order_timestamp`):
        i.  For each pending `pending_order_offset`: Read `pending_order_data`.
        ii. Check if `pending_order_data` can now be fulfilled (similar to `buy_ticket` seat check for its segments).
        iii.If yes: "buy" it (update `daily_seats.bpt`, change `pending_order_data.status` to 'S' in `orders.dat`, remove from `pending_orders_queue.bpt`).
    7.  If status is 'pending':
        a.  Update order status to 'R' in `orders.dat`.
        b.  Remove from `pending_orders_queue.bpt`.
    8.  Return 0.

*   **`query_transfer -s <S> -t <T_end> -d <D_S> (-p <sort_pref>)`**
    1.  `best_transfer_option = null`.
    2.  Convert `<D_S>` to `departure_S_date_day`.
    3.  **Find First Legs (T1: S -> M)**:
        a.  Query `station_to_trains_idx.bpt` for `<S>` -> `list_S_trains`.
        b.  For each `trainID1` in `list_S_trains`:
        i.  Fetch `T1` master data. If not `is_released`, skip.
        ii. Parse `T1` route. Find `idx_S_T1`.
        iii.Calculate `departure_time_S_T1_minutes`.
        iv. `origin_dep_date_T1 = calculate_origin_departure_date(trainID1, idx_S_T1, departure_S_date_day, departure_time_S_T1_minutes)`.
        v.  If `origin_dep_date_T1` not in `T1.saleDate` range, skip.
        vi. For each station `M` on `T1`'s route after `S` (let its index be `idx_M_T1`):
        1.  `min_seats_T1 = get_segment_seat_availability(T1_hash, origin_dep_date_T1, idx_S_T1, idx_M_T1, 1)`.
        2.  If `min_seats_T1 > 0`:
        a.  Calculate `arrival_dt_M_T1` (absolute datetime of T1 arriving at M).
        b.  Calculate `price_S_to_M_T1`.
        c.  Store `(trainID1, T1_details, M, idx_M_T1, arrival_dt_M_T1, price_S_to_M_T1, origin_dep_date_T1)` as a potential first leg.
    4.  **Find Second Legs (T2: M -> T_end) and Combine**:
        a.  For each `first_leg = (trainID1, ..., M, idx_M_T1, arrival_dt_M_T1, ...)`:
        i.  Query `station_to_trains_idx.bpt` for `M` -> `list_M_trains`.
        ii. Query `station_to_trains_idx.bpt` for `<T_end>` -> `list_T_end_trains`.
        iii.Intersect to get `common_M_Tend_trainIDs`.
        iv. For each `trainID2` in `common_M_Tend_trainIDs`:
        1.  If `trainID2 == trainID1`, skip.
        2.  Fetch `T2` master data. If not `is_released`, skip.
        3.  Parse `T2` route. Find `idx_M_T2` and `idx_T_end_T2`. If `idx_M_T2 >= idx_T_end_T2`, skip.
        4.  Calculate `departure_time_M_T2_minutes`.
        5.  Determine earliest `departure_dt_M_T2` (must be >= `arrival_dt_M_T1`).
        6.  `origin_dep_date_T2 = calculate_origin_departure_date(trainID2, idx_M_T2, departure_dt_M_T2.date(), departure_time_M_T2_minutes)`.
        7.  If `origin_dep_date_T2` not in `T2.saleDate` range, skip.
        8.  `min_seats_T2 = get_segment_seat_availability(T2_hash, origin_dep_date_T2, idx_M_T2, idx_T_end_T2, 1)`.
        9.  If `min_seats_T2 > 0`:
        a.  Calculate `arrival_dt_T_end_T2`.
        b.  Calculate `price_M_to_T_end_T2`.
        c.  Calculate total time, total price for `(T1, T2)` transfer.
        d.  Compare with `best_transfer_option` based on `-p` criteria. Update if better.
    5.  If `best_transfer_option` found, print its two legs. Else print 0.

**System Commands**

*   **`clean`**:
    1.  Delete all `.dat` and `.bpt` files created by the system.
    2.  Return 0.
*   **`exit`**:
    1.  (Ensure any B+ tree buffers are flushed if your implementation uses them).
    2.  Print "bye".
    3.  Terminate program.

This detailed breakdown should provide a solid foundation for implementation. Remember to implement robust helper functions for date/time manipulation, string parsing, and B+ tree interactions.