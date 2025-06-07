#pragma once
#include <filesystem>
#include <limits>
#include <optional>

#include "disk/IO_manager.h"
#include "disk/IO_utils.h"
#include "stlite/utils.h"
#include "stlite/vector.hpp"
#include "Node.h"
#include "common.h"
#include "my_fileconfig.h"


namespace RFlowey {

  template<typename Key, typename Value> // KeyHash removed
  class BPT {
    // key_type is now Key itself. BPTNode will use this Key directly.
    using InnerNode = BPTNode<Key, page_id_t, Inner>;
    using LeafNode = BPTNode<Key, Value, Leaf>; // Leaf node stores pair<Key, Value>

    SimpleDiskManager manager_;
    PagePtr<InnerNode> root_;
    int layer = 0; // Number of inner node levels above leaves. Root is at depth 0.

    struct BPT_config {
      bool is_set;
      int layer;
      page_id_t root_id;
    };

    RFlowey::FiledConfig::tracker_t_<BPT_config> persis_config = RFlowey::FiledConfig::track<BPT_config>(BPT_config{false,0,0});

    struct FindResult {
      pair<PageRef<LeafNode>, index_type> cur_pos; // cur_pos.second is result of BPTNode::search
      sjtu::vector<pair<PageRef<InnerNode>, index_type> > parents;
    };

    enum class OperationType { FIND, INSERT, DELETE };

    FindResult find_pos(const Key &key, OperationType type) {
#ifdef BPT_TEST
      assert(root_.page_id() != INVALID_PAGE_ID && root_.page_id() != 0 && "find_pos called with invalid root");
      assert(layer >= 0 && "find_pos called with invalid layer");
#endif
      sjtu::vector<pair<PageRef<InnerNode>, index_type> > parents;
      page_id_t next_page_id = root_.page_id();
      index_type current_path_idx; // Index used in parent vector, refers to data_[idx] in parent

      for (int i = 0; i <= layer; ++i) {
        PageRef<InnerNode> cur_inner_node = PagePtr<InnerNode>{next_page_id, &manager_}.get_ref();
#ifdef BPT_TEST
        assert(cur_inner_node->current_size_ > 0 && "Inner node on path is empty");
#endif
        current_path_idx = cur_inner_node->search(key); // BPTNode::search now compares Key directly

        if (current_path_idx == INVALID_PAGE_ID) {
          current_path_idx = 0;
        }
#ifdef BPT_TEST
        assert(current_path_idx < cur_inner_node->current_size_ && \
               "Search index out of bounds in inner node after valid return.");
#endif
        next_page_id = cur_inner_node->at(current_path_idx).second; // .second is page_id_t

        if (type != OperationType::FIND) {
          bool is_safe_for_op = (type == OperationType::INSERT && cur_inner_node->is_upper_safe()) ||
                                (type == OperationType::DELETE && cur_inner_node->is_lower_safe());
          if (is_safe_for_op) {
            parents.clear();
          }
          parents.emplace_back(std::move(cur_inner_node), current_path_idx);
        }
      }

      PageRef<LeafNode> leaf_ref = PagePtr<LeafNode>{next_page_id, &manager_}.get_ref();
      if (type != OperationType::FIND) {
        bool is_leaf_safe_for_op = (type == OperationType::INSERT && leaf_ref->is_upper_safe()) ||
                                   (type == OperationType::DELETE && leaf_ref->is_lower_safe());
        if (is_leaf_safe_for_op) {
          parents.clear();
        }
      }

      index_type idx_in_leaf = leaf_ref->search(key); // BPTNode::search compares Key directly

      return {{std::move(leaf_ref), idx_in_leaf}, std::move(parents)};
    }

  public:
    explicit BPT(const std::string &file_name): manager_(file_name), root_(INVALID_PAGE_ID, nullptr) {
      BPT_config config = persis_config.val;

      if(!config.is_set) {
#ifdef BPT_TEST
      std::cerr << "Initializing new BPT database..." << std::endl;
      assert(root_.page_id() == INVALID_PAGE_ID && "Root should be invalid before new DB init");
#endif
      PagePtr<InnerNode> new_root_ptr = allocate<InnerNode>(&manager_);
      PagePtr<LeafNode> first_leaf_ptr = allocate<LeafNode>(&manager_);

      this->layer = 0;
      this->root_ = new_root_ptr;

      // Initial leaf node contains one dummy entry with Key{}
      // LeafNode::value_type is pair<Key, Value>
      typename LeafNode::value_type initial_leaf_data[1] = {{Key{}, Value{}}};
      first_leaf_ptr.make_ref(LeafNode{first_leaf_ptr.page_id(), 1, initial_leaf_data});

      // Initial root node points to the first leaf node, using Key{} as the key
      // InnerNode::value_type is pair<Key, page_id_t>
      typename InnerNode::value_type initial_root_data[1] = {{Key{}, first_leaf_ptr.page_id()}};
      new_root_ptr.make_ref(InnerNode{new_root_ptr.page_id(), 1, initial_root_data});
    } else {
#ifdef BPT_TEST
      assert(cfg_ref->is_set && "Config page not marked as set.");
      assert(cfg_ref->layer >= 0 && "Loaded layer should be non-negative");
      assert(cfg_ref->root_id != INVALID_PAGE_ID && cfg_ref->root_id != 0 && "Loaded root_id invalid");
      assert(cfg_ref->root_id != DISK_PAGE_CONFIG_ID && "Root ID cannot be config page ID");
      std::cerr << "Loading existing BPT database..." << std::endl;
#endif
      this->root_ = PagePtr<InnerNode>{config.root_id, &manager_};
      this->layer = config.layer;
    }
  }

    ~BPT() {
#ifdef BPT_TEST
      std::cerr << "BPT Destructor: Saving config. Layer=" << layer
                << ", RootID=" << (root_.page_id() == INVALID_PAGE_ID ? -1 : root_.page_id()) << std::endl;
#endif
      if (root_.page_id() != INVALID_PAGE_ID && root_.page_id() != 0) {
        BPT_config cfg_to_save = {true, layer, root_.page_id()};
        persis_config.val = cfg_to_save;
      }
    }

    std::optional<Value> find(const Key &key) {
      auto result = find_pos(key, OperationType::FIND);
      PageRef<LeafNode>& leaf_ref = result.cur_pos.first;
      index_type index_in_leaf = result.cur_pos.second;

      if (index_in_leaf != INVALID_PAGE_ID && index_in_leaf < leaf_ref->current_size_) {
        const auto& item = leaf_ref->at(index_in_leaf); // item is pair<Key, Value>
        if (item.first == key) { // Direct key comparison
          return item.second; // Return Value
        }
      }
      return std::nullopt;
    }

    void insert(const Key &key, const Value &value) {
      auto [pos_pair, parents] = find_pos(key, OperationType::INSERT);
      PageRef<LeafNode>& leaf_ref = pos_pair.first;
      index_type search_idx_in_leaf = pos_pair.second;

      // Check if key already exists to update it
      if (search_idx_in_leaf != INVALID_PAGE_ID && search_idx_in_leaf < leaf_ref->current_size_) {
        // BPTNode::data_ is protected. To modify, PageRef needs to allow it or BPTNode needs an update method.
        // Assuming direct access for modification via PageRef for this example.
        // This might require leaf_ref.operator->()->data_[search_idx_in_leaf] or similar.
        // For simplicity, using leaf_ref->data_ as if it's directly modifiable.
        if (leaf_ref->data_[search_idx_in_leaf].first == key) {
          leaf_ref->data_[search_idx_in_leaf].second = value; // Update existing key's value
          // leaf_ref.mark_dirty(); // If PageRef requires explicit dirty marking
          return;
        }
      }

      // Key not found for update, proceed with insertion.
      // search_idx_in_leaf is the correct `pos` argument for BPTNode::insert_at.
      leaf_ref->insert_at(search_idx_in_leaf, {key, value}); // LeafNode stores pair<Key, Value>

      if (parents.empty()) {
        return;
      }
      // If parents is not empty, it implies the leaf was not upper_safe.
      // After insertion, if current_size_ >= SPLIT_T (or > SPLIT_T depending on exact definition), it must split.
      // The original code's assert in BPTNode::split is current_size_ >= SPLIT_T.
      // So, if leaf_ref->current_size_ < LeafNode::SPLIT_T, this split shouldn't happen.
      // The logic is: if parents is not empty, the leaf was not upper_safe.
      // Not upper_safe means current_size_ >= SPLIT_T - 1.
      // After insert, current_size_ >= SPLIT_T. So, split is necessary.
      if (leaf_ref->current_size_ < LeafNode::SPLIT_T) { // Check if it's actually full enough to split
          // This condition might seem redundant if `parents.empty()` check already handles safe nodes.
          // However, a node might become full but its parent is safe, stopping propagation.
          // The original code structure was: if (parents.empty()) return; then proceed to split.
          // This implies if parents is NOT empty, it ALWAYS tries to split the leaf.
          // Let's ensure it only splits if truly necessary.
          return; // Not full enough to split, or parent was safe and handled it.
      }
      // If it reaches here, parents was not empty (unsafe path) AND leaf is now full.

      PageRef<LeafNode> new_leaf_page_ref = leaf_ref->split(allocate<LeafNode>(&manager_));
      page_id_t new_node_page_id = new_leaf_page_ref->self_id_;
      Key promoted_key = new_leaf_page_ref->get_first(); // This is a Key from new leaf's data_[0].first

      // Propagate split upwards
      while (!parents.empty()) {
        PageRef<InnerNode> parent_node = std::move(parents.back().first);
        index_type insert_idx_in_parent = parents.back().second;
        parents.pop_back();

        // InnerNode stores pair<Key, page_id_t>
        parent_node->insert_at(insert_idx_in_parent, {promoted_key, new_node_page_id});

        if (parent_node->current_size_ < InnerNode::SPLIT_T) { // Parent absorbed new key, no further split
          return;
        }

        // Parent also splits
        PageRef<InnerNode> new_inner_page_ref = parent_node->split(allocate<InnerNode>(&manager_));
        new_node_page_id = new_inner_page_ref->self_id_;
        promoted_key = new_inner_page_ref->get_first(); // Key from new inner node's data_[0].first
      }

      // If loop finishes, root was split
      PagePtr<InnerNode> new_root_ptr = allocate<InnerNode>(&manager_);
      typename InnerNode::value_type new_root_data[2] = {
          {Key{}, root_.page_id()},       // Old root is first child, with sentinel Key{}
          {promoted_key, new_node_page_id} // New node from root split is second child
      };
      new_root_ptr.make_ref(InnerNode{new_root_ptr.page_id(), 2, new_root_data});
      root_ = new_root_ptr;
      ++layer;
    }

    bool erase(const Key& key) {
      auto [pos_pair, parents] = find_pos(key, OperationType::DELETE);
      PageRef<LeafNode>& leaf_ref = pos_pair.first;
      index_type found_idx_in_leaf = pos_pair.second;

      if (found_idx_in_leaf == INVALID_PAGE_ID || found_idx_in_leaf >= leaf_ref->current_size_ ||
          leaf_ref->at(found_idx_in_leaf).first != key) { // Direct key comparison
        return false; // Key not found
      }

      leaf_ref->erase(found_idx_in_leaf);

      if (parents.empty()) { // Leaf was safe or is root-leaf.
        // If root is a leaf (layer 0) and it becomes empty (except for the sentinel Key{}),
        // specific handling might be needed. Current BPTNode always keeps at least one element
        // if initialized with one, due to MERGE_T.
        // The initial dummy {Key{}, Value{}} might prevent true emptiness if MERGE_T is 0.
        // If MERGE_T >= 0, and current_size_ becomes 0, it's an issue.
        // BPTNode::erase should not let current_size_ become 0 if MERGE_T >=0 unless it's the only node.
        // The current BPT structure always has an InnerNode as root.
        return true;
      }

      // If parents is not empty, node was not lower_safe. It might underflow.
      bool needs_parent_update = false;
      if (leaf_ref->current_size_ <= LeafNode::MERGE_T) { // Check for underflow
          // BPTNode::merge merges with prev_node_id_.
          // It asserts if prev_node_id_ is INVALID_PAGE_ID.
          if (leaf_ref->prev_node_id_ != INVALID_PAGE_ID) {
              if (leaf_ref->merge(&manager_)) { // Try to merge with previous sibling
                  needs_parent_update = true; // Merge succeeded, leaf_ref is now invalid/deleted. Parent needs update.
              } else {
                  // Merge failed (e.g., redistribution happened if implemented, or prev sibling too full).
                  // No structural change to propagate upwards from this merge attempt.
                  return true;
              }
          } else {
              // No previous sibling to merge with. Could try merging/redistributing with next.
              // Current BPTNode::merge doesn't support this.
              // If it's the first child, underflowed, and no redistribution, it might stay underflowed.
              // This is a limitation of the current BPTNode::merge.
              return true;
          }
      } else {
          // Not underflowed, or no merge attempted/possible based on above.
          return true;
      }


      // If merge occurred (needs_parent_update is true)
      if(needs_parent_update) {
        while (!parents.empty()) {
            PageRef<InnerNode> parent_node = std::move(parents.back().first);
            // idx_in_parent_to_delete is the index of the (Key, Ptr) in parent
            // that pointed to the child that was merged away (the right one of the merged pair).
            index_type idx_in_parent_to_delete = parents.back().second;
            parents.pop_back();

            parent_node->erase(idx_in_parent_to_delete);

            // Handle root changes
            if (parent_node->self_id_ == root_.page_id()) {
                // If root has only one child left (which must be associated with Key{} sentinel) and layer > 0
                if (parent_node->current_size_ == 1 && layer > 0 && parent_node->at(0).first == Key{}) {
                    page_id_t old_root_page_id = parent_node->get_self();
                    root_ = PagePtr<InnerNode>{parent_node->at(0).second, &manager_}; // New root is this single child
                    --layer;
                    manager_.DeletePage(old_root_page_id); // Delete old root page
                }
                return true; // Root handled, finish.
            }

            // If parent node is underflowed and not root
            if (parent_node->current_size_ <= InnerNode::MERGE_T) {
                if (parent_node->prev_node_id_ != INVALID_PAGE_ID) {
                    if (!parent_node->merge(&manager_)) { // Try merge, if fails (e.g. rebalanced)
                        return true; // Stop propagation
                    }
                    // If merge succeeded, continue loop to update grandparent.
                } else {
                     // No previous sibling for inner node to merge with.
                    return true; // Stop propagation
                }
            } else {
                return true; // Parent not underflowed, stop.
            }
        }
      }
      return true;
    }

    sjtu::vector<pair<Key,Value>> range_find(const Key& start_key, const Key& end_key) {
        sjtu::vector<pair<Key,Value>> result_values;
        auto find_res = find_pos(start_key, OperationType::FIND);
        PageRef<LeafNode> current_leaf = std::move(find_res.cur_pos.first);
        index_type current_idx = find_res.cur_pos.second;

        if (!current_leaf.is_valid) return result_values;

        if (current_idx == INVALID_PAGE_ID) {
            current_idx = 0;
        }
        while (current_idx < current_leaf->current_size_ && current_leaf->at(current_idx).first < start_key) {
            current_idx++;
        }
        while (current_leaf.is_valid) {
            for (; current_idx < current_leaf->current_size_; ++current_idx) {
                const auto& item_key = current_leaf->at(current_idx).first;
                const auto& item_value = current_leaf->at(current_idx).second;
                if (item_key > end_key) {
                    return result_values;
                }
                result_values.push_back({item_key,item_value});
            }

            // Move to next leaf node
            if (current_leaf->next_node_id_ != INVALID_PAGE_ID) {
                current_leaf = PagePtr<LeafNode>{current_leaf->next_node_id_, &manager_}.get_ref();
                current_idx = 0; // Start scanning new leaf from the beginning
                if (!current_leaf.is_valid) break; // Should not happen if next_node_id_ was valid
            } else {
                break; // No more leaf nodes
            }
        }
        return result_values;
    }
    bool modify(const Key& key, const Value& new_value) {
      if (root_.page_id() == INVALID_PAGE_ID) return false;

      auto result = find_pos(key, OperationType::FIND);
      PageRef<LeafNode>& leaf_ref = result.cur_pos.first;
      index_type index_in_leaf = result.cur_pos.second;

      if (!leaf_ref.is_valid) return false;

      if (index_in_leaf != INVALID_PAGE_ID && index_in_leaf < leaf_ref->current_size_) {
        if (leaf_ref->data_[index_in_leaf].first == key) {
          leaf_ref->data_[index_in_leaf].second = new_value;
          return true;
        }
      }
      return false;
    }

    bool modify(const Key& key, const std::function<void(Value&)>& func) {
      if (root_.page_id() == INVALID_PAGE_ID) {
        return false; // Key cannot exist in an empty tree
      }

      auto result = find_pos(key, OperationType::FIND);
      PageRef<LeafNode>& leaf_ref = result.cur_pos.first;
      index_type index_in_leaf = result.cur_pos.second;

      if (leaf_ref.is_valid && index_in_leaf != INVALID_PAGE_ID && index_in_leaf < leaf_ref->current_size_) {
        // Assuming data_ is accessible for modification. For BPTNode, this is data_ array.
        if (leaf_ref->data_[index_in_leaf].first == key) {
          func(leaf_ref->data_[index_in_leaf].second); // Pass Value& to func
          // PageRef destructor handles writing back if dirty.
          return true; // Modification occurred
        }
      }
      return false; // Key not found or tree empty, no modification
    }

    bool range_modify(const Key& start_key, const Key& end_key, const std::function<void(Value&)>& func) {
      bool modified = false;

      if (root_.page_id() == INVALID_PAGE_ID) {
        return false;
      }

      auto find_res = find_pos(start_key, OperationType::FIND);
      PageRef<LeafNode> current_leaf = std::move(find_res.cur_pos.first);
      index_type current_idx = find_res.cur_pos.second;

      if (!current_leaf.is_valid) {
        return false;
      }

      // Adopt current_idx initialization from range_find for consistency and robustness
      if (current_idx == INVALID_PAGE_ID) {
          current_idx = 0;
      }
      while (current_idx < current_leaf->current_size_ && current_leaf->at(current_idx).first < start_key) {
          current_idx++;
      }

      page_id_t current_leaf_id = current_leaf.is_valid ? current_leaf->self_id_ : INVALID_PAGE_ID;

      while (current_leaf_id != INVALID_PAGE_ID) {
        if (!current_leaf.is_valid || current_leaf->self_id_ != current_leaf_id) {
            current_leaf = PagePtr<LeafNode>{current_leaf_id, &manager_}.get_ref();
            if (!current_leaf.is_valid) break;
            current_idx = 0; // Start scanning new leaf from the beginning
        }

        for (; current_idx < current_leaf->current_size_; ++current_idx) {
          // Use direct access for key comparison
          if (current_leaf->data_[current_idx].first > end_key) {
            current_leaf_id = INVALID_PAGE_ID; // Signal to stop outer loop
            break; // Stop processing this leaf
          }

          // Ensure the key is within the desired range [start_key, end_key]
          // The check for '> end_key' is above.
          // The check for '>= start_key' is important for all elements.
          if (current_leaf->data_[current_idx].first >= start_key) {
            // New logic: func(Value&) modifies in place.
            func(current_leaf->data_[current_idx].second);
            modified = true; // Assume func call implies a modification.
          }
        }

        if (current_leaf_id == INVALID_PAGE_ID) { // Propagate break from inner loop
            break;
        }

        current_leaf_id = current_leaf->next_node_id_;
        // current_idx will be reset to 0 if a new leaf is loaded in the next iteration's check.
      }

      return modified;
    }

    void clear() {
      manager_.Clear(); // Clears all data managed by the disk manager.

      // Re-initialize the BPT to a minimal state, identical to creating a new BPT.
      PagePtr<InnerNode> new_root_ptr = allocate<InnerNode>(&manager_);
      PagePtr<LeafNode> first_leaf_ptr = allocate<LeafNode>(&manager_);

      this->layer = 0;
      this->root_ = new_root_ptr; // Update root_ to the new root PagePtr

      // Create and write the initial leaf node.
      // LeafNode stores pair<Key, Value>.
      typename LeafNode::value_type initial_leaf_data[1] = {{Key{}, Value{}}};
      first_leaf_ptr.make_ref(LeafNode{first_leaf_ptr.page_id(), 1, initial_leaf_data});

      // Create and write the initial root node.
      // InnerNode stores pair<Key, page_id_t>.
      typename InnerNode::value_type initial_root_data[1] = {{Key{}, first_leaf_ptr.page_id()}};
      new_root_ptr.make_ref(InnerNode{new_root_ptr.page_id(), 1, initial_root_data});

#ifdef BPT_TEST
      std::cerr << "BPT cleared. New root ID: " << root_.page_id()
                << ", New first leaf ID: " << first_leaf_ptr.page_id() << std::endl;
#endif
    }

public:
  void print_tree_structure() {
#ifdef BPT_DEBUG_LOG
    std::cout << "\n====== B+Tree Structure ======" << std::endl;
    if (root_.page_id() == INVALID_PAGE_ID || root_.page_id() == 0) {
      std::cout << "Tree is empty or root is invalid." << std::endl;
      return;
    }
    std::cout << "Layer: " << layer << " (Max depth of inner node, root is 0)" << std::endl;
    std::cout << "Root Page ID: " << root_.page_id() << std::endl;
    print_node_recursive(root_.page_id(), 0);
    std::cout << "==============================\n" << std::endl;
#endif
  }

private:
#ifdef BPT_DEBUG_LOG
  // Assumes Key and Value are streamable to std::cout (i.e., operator<< is defined for them)
  void print_node_recursive(page_id_t page_id, int current_node_depth) {
    if (page_id == INVALID_PAGE_ID) {
      std::cout << std::string(current_node_depth * 4, ' ') << "INVALID_PAGE_ID" << std::endl;
      return;
    }

    std::string indent = std::string(current_node_depth * 4, ' ');
    bool current_node_is_leaf = (current_node_depth == layer + 1);

    if (!current_node_is_leaf) { // It's an InnerNode
      try {
        PageRef<InnerNode> node_ref = PagePtr<InnerNode>{page_id, &manager_}.get_ref();
        std::cout << indent << "InnerNode (ID: " << node_ref->self_id_
                  << ", Depth: " << current_node_depth
                  << ", Size: " << node_ref->current_size_
                  << ", Prev: " << node_ref->prev_node_id_
                  << ", Next: " << node_ref->next_node_id_ << "):" << std::endl;

        for (size_t i = 0; i < node_ref->current_size_; ++i) {
          auto entry = node_ref->at(i); // pair<Key, page_id_t>
          std::cout << indent << "  [" << i << "] Key: " << entry.first // Assumes Key is streamable
                    << " -> Child PID: " << entry.second << std::endl;
          print_node_recursive(entry.second, current_node_depth + 1);
        }
      } catch (const std::exception& e) {
          std::cerr << indent << "Error accessing InnerNode ID " << page_id << ": " << e.what() << std::endl;
      }
    } else { // It's a LeafNode
      try {
        PageRef<LeafNode> node_ref = PagePtr<LeafNode>{page_id, &manager_}.get_ref();
        std::cout << indent << "LeafNode (ID: " << node_ref->self_id_
                  << ", Depth: " << current_node_depth
                  << ", Size: " << node_ref->current_size_
                  << ", Prev: " << node_ref->prev_node_id_
                  << ", Next: " << node_ref->next_node_id_ << "):" << std::endl;

        for (size_t i = 0; i < node_ref->current_size_; ++i) {
          auto entry = node_ref->at(i); // pair<Key, Value>
          std::cout << indent << "  [" << i << "] Key: " << entry.first     // Assumes Key is streamable
                    << " -> Value: " << entry.second << std::endl; // Assumes Value is streamable
        }
      } catch (const std::exception& e) {
          std::cerr << indent << "Error accessing LeafNode ID " << page_id << ": " << e.what() << std::endl;
      }
    }
  }
#endif
  };
}
