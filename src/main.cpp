#include "parser.h"
#include "user.h"
#include "train.h"  // Added
#include "order.h"  // Added

#include <iostream>
#include <string>
#include <optional> // Used by UserManager interface as provided

bool TEST = false;


int main() {
    //freopen("my.out","w",stdout);

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    CommandParser parser;
    UserManager userManager;
    TrainManager trainManager; // Added
    OrderManager orderManager; // Added

    trainManager.load_id_name_mapping(); // Load station name mappings at startup

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() && std::cin.eof()) {
            break;
        }
        if (!parser.parse(line)) {
            if (line.empty() || line.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
                continue;
            }
            continue;
        }

        std::cout << "[" << parser.timestamp << "] ";

        if (parser.commandName == "add_user") {
            std::string cur_username_str = parser.getArg("c"); // May be empty if first user
            Username_t cur_username = cur_username_str;
            // For the first user, -c and -g are ignored. UserManager handles this.
            // If not the first user, -c must be provided. Parser should handle missing mandatory args if defined.
            // Assuming parser.getArg("c") returns empty string if -c is not present,
            // and UserManager::addUser handles logic for first user vs. subsequent.

            Username_t new_username = parser.getArg("u");
            Password_t password = parser.getArg("p");
            Name_t name = parser.getArg("n");
            MailAddr_t mail_addr = parser.getArg("m");
            Privilege_t privilege = -1; // Default, will be overridden
            if (parser.hasArg("g")) {
                 privilege = std::stoi(parser.getArg("g"));
            }
            // UserManager's addUser handles the logic of first user privilege being 10 and ignoring -g.

            int result = userManager.addUser(cur_username, new_username, password, name, mail_addr, privilege);
            std::cout << result << "\n";

        } else if (parser.commandName == "login") {
            Username_t username = parser.getArg("u");
            Password_t password = parser.getArg("p");
            int result = userManager.loginUser(username, password);
            std::cout << result << "\n";

        } else if (parser.commandName == "logout") {
            Username_t username = parser.getArg("u");
            int result = userManager.logoutUser(username);
            std::cout << result << "\n";

        } else if (parser.commandName == "query_profile") {
            Username_t cur_username = parser.getArg("c");
            Username_t target_username = parser.getArg("u");
            std::string result = userManager.queryProfile(cur_username, target_username);
            std::cout << result << "\n"; // Expects single line or -1, main adds newline

        } else if (parser.commandName == "modify_profile") {
            Username_t cur_username = parser.getArg("c");
            Username_t target_username = parser.getArg("u");

            std::optional<Password_t> new_password_opt;
            if (parser.hasArg("p")) new_password_opt = parser.getArg("p");

            std::optional<Name_t> new_name_opt;
            if (parser.hasArg("n")) new_name_opt = parser.getArg("n");

            std::optional<MailAddr_t> new_mail_addr_opt;
            if (parser.hasArg("m")) new_mail_addr_opt = parser.getArg("m");

            std::optional<Privilege_t> new_privilege_opt;
            if (parser.hasArg("g")) {
                new_privilege_opt = std::stoi(parser.getArg("g"));
            }

            std::string result = userManager.modifyProfile(cur_username, target_username,
                                                           new_password_opt, new_name_opt,
                                                           new_mail_addr_opt, new_privilege_opt);
            std::cout << result << "\n"; // Expects single line or -1, main adds newline

        } else if (parser.commandName == "add_train") {
            std::string train_id = parser.getArg("i");
            std::string station_num_str = parser.getArg("n");
            std::string seat_num_str = parser.getArg("m");
            std::string stations_str = parser.getArg("s");
            std::string prices_str = parser.getArg("p");
            std::string start_time_str = parser.getArg("x");
            std::string travel_times_str = parser.getArg("t");
            std::string stopover_times_str = parser.getArg("o");
            std::string sale_date_str = parser.getArg("d");
            std::string type_str = parser.getArg("y");
            int result = trainManager.add_train(train_id, station_num_str, seat_num_str, stations_str, prices_str, start_time_str, travel_times_str, stopover_times_str, sale_date_str, type_str);
            std::cout << result << "\n";

        } else if (parser.commandName == "delete_train") {
            std::string train_id = parser.getArg("i");
            int result = trainManager.delete_train(train_id);
            std::cout << result << "\n";

        } else if (parser.commandName == "release_train") {
            std::string train_id = parser.getArg("i");
            int result = trainManager.release_train(train_id);
            std::cout << result << "\n";

        } else if (parser.commandName == "query_train") {
            std::string train_id = parser.getArg("i");
            std::string date_str = parser.getArg("d");
            std::string result_str = trainManager.query_train(train_id, date_str);
            std::cout << result_str; // Expects multi-line with its own newlines

        } else if (parser.commandName == "query_ticket") {
            std::string from_station = parser.getArg("s");
            std::string to_station = parser.getArg("t");
            std::string date_str = parser.getArg("d");
            std::string sort_pref = "time"; // Default as per spec
            if (parser.hasArg("p")) {
                sort_pref = parser.getArg("p");
            }
            std::string result_str = trainManager.query_ticket(from_station, to_station, date_str, sort_pref);
            std::cout << result_str; // Expects multi-line with its own newlines

        } else if (parser.commandName == "query_transfer") {
            std::string from_station = parser.getArg("s");
            std::string to_station = parser.getArg("t");
            std::string date_str = parser.getArg("d");
            std::string sort_pref = "time"; // Default, assuming similar to query_ticket
            if (parser.hasArg("p")) {
                sort_pref = parser.getArg("p");
            }
            std::string result_str = trainManager.query_transfer(from_station, to_station, date_str, sort_pref);
            std::cout << result_str ; // Expects multi-line or "0\n", with its own newlines

        } else if (parser.commandName == "buy_ticket") {
            std::string username_str = parser.getArg("u");
            std::string train_id_str = parser.getArg("i");
            std::string date_str = parser.getArg("d");
            std::string num_tickets_str = parser.getArg("n");
            std::string from_station_str = parser.getArg("f");
            std::string to_station_str = parser.getArg("t");
            std::string queue_pref_str = "false"; // Default
            if (parser.hasArg("q")) {
                queue_pref_str = parser.getArg("q");
            }

            if (!userManager.isUserLoggedIn(username_str)) {
                std::cout << -1 << "\n";
            } else {
                std::string result_str = trainManager.buy_ticket(orderManager, parser.timestamp, username_str, train_id_str, date_str, num_tickets_str, from_station_str, to_station_str, queue_pref_str);
                std::cout << result_str << "\n"; // Expects single line (price, "queue", or -1), main adds newline
            }

        } else if (parser.commandName == "query_order") {
            std::string username_str = parser.getArg("u");
            if (!userManager.isUserLoggedIn(username_str)) {
                std::cout << -1 << "\n";
            } else {
                UsernameKey user_key(username_str.c_str());
                std::string result_str = orderManager.query_order(user_key);
                std::cout << result_str; // Expects multi-line with its own newlines
            }

        } else if (parser.commandName == "refund_ticket") {
            std::string username_str = parser.getArg("u");
            int n_val = 1;
            if (parser.hasArg("n")) {
                n_val = std::stoi(parser.getArg("n"));
            }
            if (!userManager.isUserLoggedIn(username_str)) {
                std::cout << -1 << "\n";
            } else {
                UsernameKey user_key(username_str.c_str());
                auto result = orderManager.refund_order_for_user(user_key,n_val,trainManager);
                std::cout<<result<<'\n';
            }
        } else if (parser.commandName == "clean") {
            userManager.cleanAllData();
            trainManager.clean_data(); // Added
            orderManager.clear_data(); // Added
            std::cout << 0 << "\n";

        } else if (parser.commandName == "exit") {
            userManager.handleSystemExit();
            trainManager.handle_exit(); // Persists id_to_name map
            // OrderManager data is persisted by BPTs automatically on destruction or flush.
            std::cout << "bye\n";
            break;
        } else {
             throw std::runtime_error("Unidentified command:"+parser.commandName);
        }
    }

    return 0;
}