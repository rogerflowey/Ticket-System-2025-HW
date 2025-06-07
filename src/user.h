#pragma once

#include <map.hpp>
#include <string>
#include <optional>

#include "utils.h"
#include "database.h"
#include "common.h"
#include "my_fileconfig.h"

using Username_t = std::string;
using Password_t = std::string;
using Name_t = std::string;
using MailAddr_t = std::string;
using Privilege_t = int;

using UsernameKey = RFlowey::string<21>;


struct UserData {
    RFlowey::string<21> username;
    RFlowey::hash_t password_hash;
    RFlowey::string<16> name;
    RFlowey::string<31> mailAddr;
    Privilege_t privilege;

    UserData() : privilege(0) {
    }
    Username_t getUsernameStdStr() const { return username.c_str(); }
    void setUsername(const Username_t& s) { username = s; }

    RFlowey::hash_t getPasswordHashStdStr() const { return password_hash; }
    void setPasswordHash(RFlowey::hash_t s) { password_hash = s; }

    Name_t getNameStdStr() const { return name.c_str(); }
    void setName(const Name_t& s) { name = s; }

    MailAddr_t getMailAddrStdStr() const { return mailAddr.c_str(); }
    void setMailAddr(const MailAddr_t& s) { mailAddr = s; }
};


class UserManager {
public:
    UserManager();
    ~UserManager()=default;

    int addUser(const Username_t& current_username,
                const Username_t& new_username,
                const Password_t& password,
                const Name_t& name,
                const MailAddr_t& mail_addr,
                Privilege_t privilege);

    int loginUser(const Username_t& username,
                  const Password_t& password);

    int logoutUser(const Username_t& username);

    std::string queryProfile(const Username_t& current_username,
                             const Username_t& target_username);

    std::string modifyProfile(const Username_t& current_username,
                              const Username_t& target_username,
                              const std::optional<Password_t>& new_password_opt,
                              const std::optional<Name_t>& new_name_opt,
                              const std::optional<MailAddr_t>& new_mail_addr_opt,
                              std::optional<Privilege_t> new_privilege_opt);

    void handleSystemExit();
    void cleanAllData();

    bool isUserLoggedIn(const Username_t& username_str);

private:
    constexpr static std::string db_file_prefix = "user_data";

    HashedSingleMap<UsernameKey, UserData, RFlowey::hasher<21>> user_data_map_;
    sjtu::map<RFlowey::hash_t,bool> online_users_map_;

    RFlowey::FiledConfig::tracker_t_<bool> is_first_user = RFlowey::FiledConfig::track<bool>(true);

    std::optional<UserData> getUserRecord(const Username_t& username_str);


    [[nodiscard]] RFlowey::hash_t hashPassword(const Password_t& password) const;
    [[nodiscard]] std::string formatUserProfile(const UserData& record) const;
};