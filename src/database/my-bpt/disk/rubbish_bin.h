#pragma once

#include <vector>
#include "common.h"

namespace RFlowey {
  class RubbishBin {
    std::vector<page_id_t> free_page_;
  public:
    RubbishBin() {

    }
    bool empty() {
      return free_page_.empty();
    }
    void push(page_id_t page_id) {
      free_page_.push_back(page_id);
    }
    page_id_t pop() {
      auto back = free_page_.back();
      free_page_.pop_back();
      return back;
    }
  };
}