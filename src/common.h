#ifndef COMMON_HPP
#define COMMON_HPP

#include <string> // Allowed STL container
#include "database/include/b_plus_tree.hpp"

template<typename idx_t,typename value_t>
using BPlusTree = norb::BPlusTree<idx_t,value_t>;

const int USERNAME_LEN = 21;
const int PASSWORD_HASH_LEN = 65;
const int NAME_LEN = 16;
const int MAIL_ADDR_LEN = 31;
const int TRAIN_ID_LEN = 21;
const int STATION_NAME_LEN = 31;
const int MAX_STATIONS_ON_ROUTE = 100;

template<int Size>
struct FixedString {
  char data[Size];
  FixedString() { data[0] = '\0'; }
  FixedString(const std::string& s) {
    strncpy(data, s.c_str(), Size -1);
    data[Size-1] = '\0';
  }
  bool operator<(const FixedString<Size>& other) const { return strcmp(data, other.data) < 0; }
  bool operator==(const FixedString<Size>& other) const { return strcmp(data, other.data) == 0; }
};

#endif // COMMON_HPP