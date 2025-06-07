#pragma once
#include <string>
#include <functional>
#include <utility>
#include <vector>
#include <optional>
#include "my-bpt/BPT.h"


using hash_t = RFlowey::hash_t;

template<
  typename T,
  typename PrimaryHasher,
  size_t NumHashFunctions = 3, // Number of hash functions
  size_t BitArraySize = 1024 * 8 // Size of the bit array (e.g., 1KB)
>
class BloomFilter {

public:
  BloomFilter() : bits_(BitArraySize, false), filepath_(""), primary_hasher_() {
  }

  explicit BloomFilter(std::string filepath)
    : bits_(BitArraySize, false), filepath_(std::move(filepath)), primary_hasher_() {
    if (!filepath_.empty()) {
      std::ifstream ifs(filepath_, std::ios::binary);
      if (ifs.is_open()) {
        if (!this->load(ifs)) {
          this->clear();
        }
        ifs.close();
      }
    }
  }

  ~BloomFilter() {
    if (!filepath_.empty()) {
      std::ofstream ofs(filepath_, std::ios::binary | std::ios::trunc);
      if (ofs.is_open()) {
        if (!this->save(ofs)) {
        }
        ofs.close();
      } else {
        std::cerr << "Warning: BloomFilter could not open file for saving: " << filepath_ << std::endl;
      }
    }
  }


  void add(const T &item) {
    std::size_t h1 = primary_hasher_(item);
    for (size_t i = 0; i < NumHashFunctions; ++i) {
      std::size_t hash_val = (h1 + i * (h1 / (11 + i * i) + 1)) % BitArraySize; // Example perturbation
      bits_[hash_val] = true;
    }
  }

  bool might_contain(const T &item) const {

    cnt++;
    std::size_t h1 = primary_hasher_(item);
    for (size_t i = 0; i < NumHashFunctions; ++i) {
      std::size_t hash_val = (h1 + i * (h1 / (11 + i * i) + 1)) % BitArraySize;
      if (!bits_[hash_val]) {
        hit++;
        return false;
      }
    }
    return true;
  }

  void clear() {
    bits_.assign(BitArraySize, false);
  }

  bool save(std::ostream &os) const {
    std::cerr<<"Hit rate:"<<hit<<'/'<<cnt<<'\n';
    for (size_t i = 0; i < BitArraySize; ++i) {
      char bit = bits_[i] ? '1' : '0';
      if (!os.put(bit)) return false;
    }
    return os.good();
  }

  bool load(std::istream &is) {
    char bit_char;
    for (size_t i = 0; i < BitArraySize; ++i) {
      if (!is.get(bit_char)) {
        return false;
      }
      if (bit_char == '1') {
        bits_[i] = true;
      } else if (bit_char == '0') {
        bits_[i] = false;
      } else {
        return false;
      }
    }
    return is.good() || is.eof();
  }

private:
  std::vector<bool> bits_;
  [[no_unique_address]] PrimaryHasher primary_hasher_;
  std::string filepath_;
  inline static size_t cnt=0;
  inline static size_t hit=0;
};

template<typename Key, typename Value>
class SingleMap {
public:
  using BPlusTree = RFlowey::BPT<Key, Value>;
  BPlusTree bpt;

  explicit SingleMap(const std::string &path): bpt(path) {
  }

  void insert(const Key &key, const Value &value) { bpt.insert(key, value); }
  void erase(const Key &key) { bpt.erase(key); }
  std::optional<Value> find(const Key &key) { return bpt.find(key); }

  sjtu::vector<RFlowey::pair<Key, Value> > find_range(const Key &start, const Key &end) {
    return bpt.range_find(start, end);
  }

  bool modify(const Key &key, const Value &new_value) {
    return bpt.modify(key, new_value);
  }

  bool modify(const Key &key, const std::function<void(Value&)>& func) {
    return bpt.modify(key, func);
  }

  bool range_modify(const Key &start_key, const Key &end_key, const std::function<void(Value&)>& func) {
    return bpt.range_modify(start_key, end_key, func);
  }

  void clear() {
    bpt.clear();
  }
};

template<typename Key, typename Value, typename Hash = std::hash<Key> >
class HashedSingleMap {
public:
  using BPlusTree = RFlowey::BPT<hash_t, Value>;
  BPlusTree bpt;
  //BloomFilter<hash_t,RFlowey::hashHasher> filter;
  [[no_unique_address]] Hash hash_func;

  explicit HashedSingleMap(const std::string &path): bpt(path) {
  }

  void insert(const Key &key, const Value &value) {
    bpt.insert(hash_func(key), value);
  }
  void erase(const Key &key) { bpt.erase(hash_func(key)); }
  std::optional<Value> find(const Key &key) {
    return bpt.find(hash_func(key));
  }
  std::optional<Value> find_by_hash(const hash_t &hashed_key) {
    return bpt.find(hashed_key);
  }

  bool modify(const Key &key, const Value &new_value) {
    return bpt.modify(hash_func(key), new_value);
  }

  bool modify_by_hash(const hash_t &hashed_key, const Value &new_value) {
    return bpt.modify(hashed_key, new_value);
  }

  bool modify(const Key &key, const std::function<void(Value&)>& func) {
    return bpt.modify(hash_func(key), func);
  }

  bool modify_by_hash(const hash_t &hashed_key, const std::function<void(Value&)>& func) {
    return bpt.modify(hashed_key, func);
  }
  void clear() {
    bpt.clear();
  }
};

template<typename Key, typename Value>
class OrderedMultiMap {
public:
  using BPlusTree = RFlowey::BPT<RFlowey::pair<Key, Value>, RFlowey::Nothing>;
  BPlusTree bpt;

  explicit OrderedMultiMap(const std::string &path): bpt(path) {
  }

  void insert(const Key &key, const Value &value) {
    bpt.insert({key, value}, RFlowey::Nothing{});
  }

  void erase(const Key &key, const Value &value) {
    bpt.erase({key, value});
  }

  sjtu::vector<RFlowey::pair<Key, Value> > find_range(const Key &start_k, const Key &end_k) {
    sjtu::vector<RFlowey::pair<Key, Value> > return_val;
    auto bpt_result_vec = bpt.range_find(
      RFlowey::pair<Key, Value>{start_k, Value{}},
      RFlowey::pair<Key, Value>{end_k, Value{}}
    );
    for (const auto &bpt_pair_entry: bpt_result_vec) {
      if (bpt_pair_entry.first.first >= start_k && bpt_pair_entry.first.first <= end_k) {
        return_val.push_back(bpt_pair_entry.first);
      }
    }
    return return_val;
  }
  void clear() {
    bpt.clear();
  }
};

template<typename Key, typename Value, typename Hash = std::hash<Key> >
class OrderedHashMap {
public:
  using BPlusTree = RFlowey::BPT<RFlowey::pair<hash_t, Value>, RFlowey::Nothing>;
  BPlusTree bpt;
  //BloomFilter<hash_t,RFlowey::hashHasher> filter;
  [[no_unique_address]] Hash hash_func;

  explicit OrderedHashMap(const std::string &path): bpt(path) {
  }

  ~OrderedHashMap() = default;

  void insert(const Key &key, const Value &value) {
    bpt.insert({hash_func(key), value}, RFlowey::Nothing{});
  }

  void erase(const Key &key, const Value &value) {
    bpt.erase({hash_func(key), value});
  }

  sjtu::vector<Value> find(const Key &key) {
    return find_by_hash(hash_func(key));
  }

  sjtu::vector<Value> find_by_hash(const hash_t &hashed_key) {
    auto result_bpt = bpt.range_find({hashed_key, Value{}}, {hashed_key + 1, Value{}});
    sjtu::vector<Value> return_val;
    for (auto &i: result_bpt) {
      if (i.first.first == hashed_key) {
        return_val.push_back(i.first.second);
      }
    }
    return return_val;
  }
  void clear() {
    bpt.clear();
  }
};
