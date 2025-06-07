#include "IO_utils.h"

#include <utility>
#include "IO_manager.h"
#include "serialize.h"

namespace RFlowey {
  Page::Page(IOManager* manager,page_id_t page_id):manager_(manager), page_id_(page_id){};

  Page::~Page() {
    flush();
  };

  char* Page::get_data() {
    return data_;
  }
  void Page::flush() {
    if(manager_) {
      manager_->WritePage(*this,page_id_);
    }
  }



}

