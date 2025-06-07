#pragma once

#include <cassert>

#include <cstdint>
#include <cstring>
#include <fstream>

namespace norb {

  namespace filesystem {
    // Check whether a fstream is empty.
    inline bool is_empty(std::fstream &f) {
      const std::streampos pos = f.tellg();
      // Change Note: using seek(0) on an empty file will disrupt the fstream f.
      f.seekg(0, std::ios::beg);
      const auto begin = f.tellg();
      f.seekg(0, std::ios::end);
      const auto end = f.tellg();
      f.seekg(pos);
      assert(f.good());
      return begin == end;
    }

    // Create file when it doesn't exist.
    // ! Can only be called when no other fstreams are pointing to that file !
    inline void fassert(const std::string &path) {
      std::fstream f;
      f.open(path, std::ios::app);
      f.close();
    }

    // Get the size of a file in bytes
    inline unsigned long get_size(const std::string &path) {
      std::fstream f(path, std::ios::in | std::ios::ate);
      return f.tellg();
    }

    // Binary read from a file.
    template <typename T_> void binary_read(std::fstream &f, T_ &item) {
      f.read(reinterpret_cast<char *>(&item), sizeof(item));
    }

    // Binary write into a file.
    template <typename T_> void binary_write(std::fstream &f, T_ &item) {
      f.write(reinterpret_cast<char *>(&item), sizeof(item));
    }

    // Binary write into a file.
    template <typename T_>
    void binary_write(std::fstream &f, T_ &item, const int size_) {
      f.write(reinterpret_cast<char *>(&item), size_);
    }

    // Clear all texts within a stream and reopen it.
    inline void trunc(std::fstream &f, const std::string &file_name,
                      const std::_Ios_Openmode mode) {
      f.close();
      remove(file_name.c_str());
      fassert(file_name);
      f.open(file_name, mode);
    }
  } // namespace filesystem

  namespace hash {
    // a collection of commonly used hashing algorithms
    using hashed_t_ = unsigned long long;

    inline hashed_t_ basic_hash(const std::string &str) {
      constexpr hashed_t_ MOD = 4294967029;
      hashed_t_ hash = 0;
      for (auto i : str) {
        hash += static_cast<hashed_t_>(i);
        hash = (hash << 16) + hash;
        hash %= MOD;
      }
      return hash;
    }

    inline hashed_t_ fnv1a_hash(const std::string &str) {
      // FNV constants for 32-bit hash
      constexpr hashed_t_ fnv_prime = 16777619U;
      constexpr hashed_t_ fnv_offset_basis = 2166136261U;

      hashed_t_ hash = fnv_offset_basis;

      // Iterate over bytes as unsigned char
      for (unsigned char c : str) {
        hash ^= static_cast<hashed_t_>(c); // XOR with byte
        hash *= fnv_prime;                 // Multiply by prime
      }
      return hash;
    }

    inline hashed_t_ djb2_hash(const std::string &str) {
      hashed_t_ hash = 5381; // Initial magic constant
      // Iterate over bytes as unsigned char
      for (unsigned char c : str) {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + static_cast<hashed_t_>(c);
      }
      return hash;
    }
  } // namespace hash
} // namespace norb

struct nothing{};