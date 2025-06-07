#pragma once

#include <utility>
#include <cstring>
#include <cassert>
#include <string>
#include <my-bpt/common.h>

#include "norb_utils.hpp"


namespace RFlowey {
  //from STLite
  template<class T1, class T2>
  class pair {
  public:
    T1 first; // first    element
    T2 second; // second   element

    constexpr pair() = default;

    constexpr pair(const pair &other) = default;

    constexpr pair(pair &&other) = default;

    constexpr pair &operator=(const pair &other) = default;

    constexpr pair &operator=(pair &&other) = default;

    template<class U1 = T1, class U2 = T2>
    constexpr pair(U1 &&x, U2 &&y)
      : first(std::forward<U1>(x)), second(std::forward<U2>(y)) {
    }

    template<class U1, class U2>
    constexpr pair(const pair<U1, U2> &other)
      : first(other.first), second(other.second) {
    }

    template<class U1, class U2>
    constexpr pair(pair<U1, U2> &&other)
      : first(std::move(other.first))
        , second(std::move(other.second)) {
    }
  };

  template<class T1, class T2>
  pair(T1, T2) -> pair<T1, T2>;

  template<class T1, class T2>
  constexpr bool operator==(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
  }

  template<class T1, class T2>
  constexpr bool operator!=(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    return !(lhs == rhs);
  }

  template<class T1, class T2>
  constexpr bool operator<(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    if (lhs.first < rhs.first) return true;
    if (rhs.first < lhs.first) return false;
    return lhs.second < rhs.second;
  }

  template<class T1, class T2>
  constexpr bool operator<=(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    return !(rhs < lhs);
  }

  template<class T1, class T2>
  constexpr bool operator>(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    return rhs < lhs;
  }

  template<class T1, class T2>
  constexpr bool operator>=(const pair<T1, T2> &lhs, const pair<T1, T2> &rhs) {
    return !(lhs < rhs);
  }

  template<class T1, class T2>
  pair<T1, T2> make_pair(const T1 &t1, const T2 &t2) {
    return {t1, t2};
  }

  template<int N>
  struct string {
    char a[N]{};

    string() noexcept {
      std::memset(a, 0, N);
    }

    string(const std::string &s) noexcept {
      assign(s.data(), s.length());
    }

    string(const char *c_str) noexcept {
      if (c_str == nullptr) {
        std::memset(a, 0, N);
      } else {
        size_t len = strnlen(c_str, N);
        assign(c_str, len);
      }
    }

    // Copy constructor (defaulted is fine)
    string(const string &other) noexcept = default;

    // Move constructor (defaulted is fine and implicitly noexcept)
    string(string &&other) noexcept = default;


    // --- Assignment Operators ---

    // Copy assignment operator (defaulted is fine)
    string &operator=(const string &other) noexcept = default;

    // Move assignment operator (defaulted is fine and implicitly noexcept)
    string &operator=(string &&other) noexcept = default;

    // Assignment from std::string
    string &operator=(const std::string &s) noexcept {
      assign(s.data(), s.length());
      return *this;
    }

    string &operator=(const char *c_str) noexcept {
      if (c_str == nullptr) {
        std::memset(a, 0, N); // Treat nullptr as empty string
      } else {
        size_t len = strnlen(c_str, N);
        assign(c_str, len);
      }
      return *this;
    }


    [[nodiscard]] size_t length() const noexcept {
      return strnlen(a, N);
    }

    [[nodiscard]] bool empty() const noexcept {
      return a[0] == '\0';
    }


    static constexpr size_t capacity() noexcept {
      return N;
    }

    [[nodiscard]] std::string get_str() const {
      return std::string(a, length());
    }

    [[nodiscard]] const char *c_str() const noexcept {
      return a;
    }

    char *c_str() noexcept {
      return a;
    }

    [[nodiscard]] const char *data() const noexcept {
      return a;
    }

    char *data() noexcept {
      return a;
    }

    bool operator==(const string<N> &other) const noexcept {
      return std::strncmp(a, other.a, N) == 0;
    }

    bool operator!=(const string<N> &other) const noexcept {
      return !(*this == other);
    }

    bool operator<(const string<N> &other) const noexcept {
      return std::strncmp(a, other.a, N) < 0;
    }

    bool operator<=(const string<N> &other) const noexcept {
      return std::strncmp(a, other.a, N) <= 0;
    }

    bool operator>(const string<N> &other) const noexcept {
      return std::strncmp(a, other.a, N) > 0;
    }

    bool operator>=(const string<N> &other) const noexcept {
      return std::strncmp(a, other.a, N) >= 0;
    }

    hash_t hash() const{
      hash_t hash = 5381;
      char c;
      for (int i = 0; i < N; ++i) {
        c = c_str()[i];
        hash = ((hash << 5) + hash) + static_cast<hash_t>(c);
      }
      return hash;
    }

  private:
    void assign(const char *data_ptr, size_t len) noexcept {
      size_t bytes_to_copy = std::min(len, static_cast<size_t>(N));
      std::memcpy(a, data_ptr, bytes_to_copy);

      if (bytes_to_copy < N) {
        // Zero-pad the rest of the buffer
        std::memset(a + bytes_to_copy, 0, N - bytes_to_copy);
      }
    }
  };

  struct Nothing {
  };


  inline hash_t hash(const std::string &str) {
    return norb::hash::djb2_hash(str);
  }

  inline hash_t hash(hash_t hash) {
    return hash ^ (hash * 37);
  }

  template<size_t N>
  struct hasher {
    hash_t operator()(const string<N> &str) {
      hash_t hash = 5381;
      char c;
      for (int i = 0; i < N; ++i) {
        c = str.c_str()[i];
        hash = ((hash << 5) + hash) + static_cast<hash_t>(c);
      }
      return hash;
    }
  };

  struct hashHasher {
    hash_t operator()(hash_t hash) const {
      return hash ^ (hash * 137);
    }
  };

  template<typename RandomAccessIterator, typename Compare>
  RandomAccessIterator lomuto_partition_impl(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    RandomAccessIterator pivot_iter = last - 1;
    RandomAccessIterator i = first;

    for (RandomAccessIterator j = first; j != pivot_iter; ++j) {
      if (comp(*j, *pivot_iter)) {
        std::swap(*i, *j);
        ++i;
      }
    }
    std::swap(*i, *pivot_iter);
    return i;
  }

  template<typename RandomAccessIterator, typename Compare>
  void quick_sort_recursive_impl(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    if (last - first < 2) {
      return;
    }

    RandomAccessIterator pivot_position = lomuto_partition_impl(first, last, comp);
    quick_sort_recursive_impl(first, pivot_position, comp);
    quick_sort_recursive_impl(pivot_position + 1, last, comp);
  }

  template<typename RandomAccessIterator, typename Compare>
  void quick_sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    if (first == last) {
      // Handle empty range explicitly, though recursive impl would too.
      return;
    }
    quick_sort_recursive_impl(first, last, comp);
  }
}
