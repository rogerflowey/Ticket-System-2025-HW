#ifndef NODE_H
#define NODE_H

#include <optional>
#include <variant>

#include "disk/IO_manager.h"
#include "disk/IO_utils.h"
#include "stlite/utils.h"
#include "common.h"
#include "disk/serialize.h"


namespace RFlowey {

  enum PAGETYPE:char {
    Leaf, Inner
  };

  template<typename Key, typename Value,PAGETYPE type>
  class BPTNode {
  public:
    using value_type = pair<Key,Value>;
#ifndef BPT_SMALL_SIZE
    static constexpr int SIZEMAX = (PAGESIZE - 48) / sizeof(value_type);
#else
    static constexpr int SIZEMAX = 12;
#endif
    static constexpr int SPLIT_T = SPLIT_RATE*SIZEMAX-1;
    static constexpr int MERGE_T = MERGE_RATE*SIZEMAX-1;
    static_assert(SIZEMAX>=8);

    page_id_t self_id_=INVALID_PAGE_ID;
    page_id_t prev_node_id_=INVALID_PAGE_ID;
    page_id_t next_node_id_=INVALID_PAGE_ID;
    size_t current_size_=0;

    value_type data_[SIZEMAX];


  public:
    BPTNode(page_id_t self_id,size_t current_size,value_type data[]):self_id_(self_id),current_size_(current_size) {
      std::memcpy(data_,data,current_size*sizeof(value_type));
    }
    BPTNode(page_id_t self_id) : self_id_(self_id), current_size_(0) {}
    /**
     * @brief binary search for the key
     * @return the last index <= key
     */
    [[nodiscard]] index_type search(const Key &key) const {
      index_type l = 0, r = current_size_;
      while (l < r) {
        index_type mid = l + (r - l) / 2;
        if (data_[mid].first <= key) {
          l = mid + 1;
        } else {
          r = mid;
        }
      }
      if(l==0) {
        return INVALID_PAGE_ID;//this is only for readability since INVALID_PAGE_ID is -1;
      }
      return l-1;
    }

    [[nodiscard]] value_type at(index_type pos) const {
#ifdef BPT_TEST
      if (pos >= current_size_) {
        throw std::out_of_range("BPTNode::at: position out of bounds");
      }
#endif
      return data_[pos];
    }
    Key& head(index_type pos) {
#ifdef BPT_TEST
      if (pos >= current_size_) {
        throw std::out_of_range("BPTNode::head: position out of bounds");
      }
#endif
      return data_[pos].first;
    }
    Key get_first() {
#ifdef BPT_TEST
      if (current_size_ == 0) {
        throw std::logic_error("BPTNode::get_first: node is empty");
      }
#endif
      return data_[0].first;
    }

    /**
     * @brief insert key,value after pos.;
     */
    void insert_at(index_type pos,const value_type& value) {
#ifdef BPT_TEST
      if (current_size_ >= SIZEMAX) {
        throw std::overflow_error("BPTNode::insert_at: node is full");
      }
#endif
      std::memmove(data_+pos+2,data_+pos+1,sizeof(value_type)*(current_size_-(pos+1)));
      data_[pos+1] = value;
      current_size_++;
    }

    /**
     * @brief erase the data at pos. pos should be at least 0;
     */
    void erase(index_type pos) {
#ifdef BPT_TEST
      if (current_size_ == 0) {
        throw std::out_of_range("BPTNode::erase: position out of bounds");
      }
      if (pos >= current_size_) {
        throw std::out_of_range("BPTNode::erase: position out of bounds");
      }
#endif
      std::memmove(data_+pos,data_+pos+1,sizeof(value_type)*(current_size_-pos-1));
      current_size_--;
    }

    [[nodiscard]] bool is_safe() const {
      return current_size_<SPLIT_T-1 && current_size_>MERGE_T+1;
    }
    [[nodiscard]] bool is_upper_safe() const {
      return current_size_<SPLIT_T-1;
    }
    [[nodiscard]] bool is_lower_safe() const {
      return current_size_>MERGE_T+1 || prev_node_id_==INVALID_PAGE_ID;
    }

    PageRef<BPTNode> split(const PagePtr<BPTNode>& ptr) {
#ifdef BPT_TEST
      assert(current_size_>=SPLIT_T);
#endif
      std::unique_ptr<BPTNode> temp = std::make_unique<BPTNode>(*this);

      temp->prev_node_id_ = self_id_;
      temp->next_node_id_ = next_node_id_;
      if (next_node_id_ != INVALID_PAGE_ID) {
        PagePtr<BPTNode>{next_node_id_,ptr.manager_}.get_ref()->prev_node_id_ = ptr.page_id();
      }
      next_node_id_ = ptr.page_id();
      temp->self_id_ = ptr.page_id();


      int mid = current_size_/2;
      std::memmove(temp->data_,temp->data_+mid,(current_size_-mid)*sizeof(value_type));
      temp->current_size_ = current_size_-mid;
      current_size_ = mid;

      return ptr.make_ref(std::move(temp));
    }

    bool merge(IOManager* manager) {
#ifdef BPT_TEST
      assert(prev_node_id_!=INVALID_PAGE_ID);
#endif
      auto prev_node = PagePtr<BPTNode>{prev_node_id_,manager}.get_ref();
      if(prev_node->current_size_+current_size_>=SIZEMAX-1) {
        return false;
      }
      if(next_node_id_!=INVALID_PAGE_ID) {
        auto next_node = PagePtr<BPTNode>{next_node_id_,manager}.get_ref();
        next_node->prev_node_id_ = prev_node_id_;
      }
      std::memcpy(prev_node->data_+prev_node->current_size_,data_,current_size_*sizeof(value_type));
      prev_node->current_size_ += current_size_;
      prev_node->next_node_id_ = next_node_id_;
      manager->DeletePage(self_id_);
      return true;
    }



    [[nodiscard]] page_id_t get_self() const {
      return self_id_;
    }

  };
}

#endif //NODE_H
