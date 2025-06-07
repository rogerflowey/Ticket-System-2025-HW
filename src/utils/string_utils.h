#pragma once
#include <string>
#include <sstream>

#include "stlite/vector.hpp"


inline sjtu::vector<std::string> split(const std::string& str, char delimiter) {
  sjtu::vector<std::string> tokens;
  std::string token;
  std::stringstream ss(str); // Create a stringstream from the input string

  // Use getline to extract tokens separated by the delimiter
  while (std::getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}