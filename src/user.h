#ifndef USER_MANAGER_HPP
#define USER_MANAGER_HPP

#include "common.h"
#include <fstream>

struct UserRecord {
    char username[USERNAME_LEN];
    char password_hash[PASSWORD_HASH_LEN];
    char name[NAME_LEN];
    char mailAddr[MAIL_ADDR_LEN];
    int privilege;

    UserRecord();
    UserRecord(const FixedString<USERNAME_LEN>& u, const FixedString<PASSWORD_HASH_LEN>& ph,
               const FixedString<NAME_LEN>& n, const FixedString<MAIL_ADDR_LEN>& m, int p);
};

class UserManager {
private:
    std::fstream users_file;
    const std::string users_filename = "users.dat";
    const std::string users_idx_filename = "users_username_idx.bpt";
    const std::string online_users_idx_filename = "online_users.bpt";

    void* users_username_idx_ptr;
    void* online_users_idx_ptr;

    bool is_system_empty;

    bool readUserRecord(long offset, UserRecord& record_out);
    bool writeUserRecord(long offset, const UserRecord& record_in);
    long appendUserRecord(const UserRecord& record_in);
    std::string hashPassword(const std::string& password);

public:
    UserManager();
    ~UserManager();

    void init();
    void clean();

    int addUser(const std::string& current_username,
                const std::string& new_username,
                const std::string& password,
                const std::string& name,
                const std::string& mailAddr,
                int privilege_to_set);

    int loginUser(const std::string& username, const std::string& password);
    int logoutUser(const std::string& username);

    int queryProfile(const std::string& current_username,
                     const std::string& target_username,
                     std::string& profile_data_out);

    int modifyProfile(const std::string& current_username,
                      const std::string& target_username,
                      const std::string* new_password,
                      const std::string* new_name,
                      const std::string* new_mailAddr,
                      const int* new_privilege,
                      std::string& modified_profile_data_out);

    bool isUserLoggedIn(const std::string& username);
    bool getUserPrivilege(const std::string& username, int& privilege_out);
    bool findUser(const std::string& username, UserRecord& record_out, long& offset_out);
};

#endif // USER_MANAGER_HPP