#include "user.h"

UserManager::UserManager(): user_data_map_(db_file_prefix+".dat") {
}

int UserManager::addUser(const Username_t &current_username, const Username_t &new_username, const Password_t &password, const Name_t &name, const MailAddr_t &mail_addr, Privilege_t privilege) {
  if(!is_first_user.val) {
    auto result = getUserRecord(current_username);
    if(!result) {
      return -1;
    }
    if(result->privilege<=privilege || !isUserLoggedIn(current_username)) {
      return -1;
    }
    if(getUserRecord(new_username)) {
      return -1;
    }
  } else {
    privilege = 10;
  }
  UserData new_user;
  new_user.setUsername(new_username);
  new_user.setPasswordHash(hashPassword(password));
  new_user.setName(name);
  new_user.setMailAddr(mail_addr);
  new_user.privilege = privilege;
  user_data_map_.insert(new_username,new_user);
  is_first_user.val = false;
  return 0;
}

int UserManager::loginUser(const Username_t &username, const Password_t &password) {
  auto result = getUserRecord(username);
  if(!result) {
    return -1;
  }
  UserData user = result.value();
  if(user.password_hash!=hashPassword(password)) {
    return -1;
  }
  if(isUserLoggedIn(username)) {
    return -1;
  }
  online_users_map_.insert({RFlowey::hash(username),true});
  return 0;
}

int UserManager::logoutUser(const Username_t &username) {
  if(!isUserLoggedIn(username)) {
    return -1;
  }
  online_users_map_.erase(RFlowey::hash(username));
  return 0;
}

std::string UserManager::queryProfile(const Username_t &current_username, const Username_t &target_username) {
  if(!isUserLoggedIn(current_username)) {
    return "-1";
  }
  auto cur_user_opt = getUserRecord(current_username);
  auto tar_user_opt = getUserRecord(target_username);
  if(!cur_user_opt||!tar_user_opt) {
    return "-1";
  }
  UserData& cur_user = cur_user_opt.value();
  UserData& tar_user = tar_user_opt.value();
  if(cur_user.privilege<=tar_user.privilege &&current_username!=target_username) {
    return "-1";
  }
  return formatUserProfile(tar_user);
}

std::string UserManager::modifyProfile(const Username_t &current_username, const Username_t &target_username, const std::optional<Password_t> &new_password_opt, const std::optional<Name_t> &new_name_opt, const std::optional<MailAddr_t> &new_mail_addr_opt, std::optional<Privilege_t> new_privilege_opt) {
  if(!isUserLoggedIn(current_username)) {
    return "-1";
  }
  auto cur_user_opt = getUserRecord(current_username);
  auto tar_user_opt = getUserRecord(target_username);
  if(!cur_user_opt||!tar_user_opt) {
    return "-1";
  }
  UserData& cur_user = cur_user_opt.value();
  UserData& tar_user = tar_user_opt.value();
  if(cur_user.privilege<=tar_user.privilege&&current_username!=target_username) {
    return "-1";
  }
  if(new_privilege_opt) {
    auto new_privilege = new_privilege_opt.value();
    if(new_privilege>=cur_user.privilege) {
      return "-1";
    }
    tar_user.privilege = new_privilege;
  }
  
  if(new_password_opt) {
    tar_user.setPasswordHash(hashPassword(new_password_opt.value()));
  }
  if(new_name_opt) {
    tar_user.setName(new_name_opt.value());
  }
  if(new_mail_addr_opt) {
    tar_user.setMailAddr(new_mail_addr_opt.value());
  }

  user_data_map_.erase(target_username);
  user_data_map_.insert(target_username,tar_user);
  return formatUserProfile(tar_user);
}

void UserManager::handleSystemExit() {
  online_users_map_.clear();
}

void UserManager::cleanAllData() {
  is_first_user.val = true;
  user_data_map_.clear();
  online_users_map_.clear();
}

std::optional<UserData> UserManager::getUserRecord(const Username_t &username_str) {
  return user_data_map_.find(username_str);
}

bool UserManager::isUserLoggedIn(const Username_t &username_str) {
  return (online_users_map_.find(RFlowey::hash(username_str))!=online_users_map_.end());
}

RFlowey::hash_t UserManager::hashPassword(const Password_t &password) const {
  return norb::hash::djb2_hash(password);
}

std::string UserManager::formatUserProfile(const UserData &record) const {
  return record.getUsernameStdStr()+" "+record.getNameStdStr()+" "+record.getMailAddrStdStr()+" "+std::to_string(record.privilege);
}

