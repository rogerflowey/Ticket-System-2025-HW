#pragma once

#include <fstream>
#include <memory>
#include "common.h"


namespace RFlowey {
  class Page;

  class IOManager {
  public:
    virtual ~IOManager();

    virtual page_id_t NewPage() = 0;
    virtual void DeletePage(page_id_t page_id) = 0;
    virtual std::shared_ptr<Page> ReadPage(page_id_t page_id) = 0;
    virtual void WritePage(Page& page,page_id_t page_id) = 0;
    virtual void Clear() = 0;
  };

  class MemoryManager:public IOManager {
    char memory_[4*1024*1024]={};
    page_id_t next_page_=0;

  public:
    bool is_new = true;
    MemoryManager() = default;
    explicit MemoryManager(const std::string& file_name);

    page_id_t NewPage() override;
    void DeletePage(page_id_t page_id) override;
    std::shared_ptr<Page> ReadPage(page_id_t page_id) override;
    void WritePage(Page &page, page_id_t page_id) override;
    void Clear() override;
  };

  class SimpleDiskManager:public IOManager {
    std::fstream file_;
    page_id_t next_page_=1;//0 reserved

  public:
    bool is_new = true;
    explicit SimpleDiskManager(const std::string& file_name);
    ~SimpleDiskManager() override;

    page_id_t NewPage() override;
    void DeletePage(page_id_t page_id) override;
    std::shared_ptr<Page> ReadPage(page_id_t page_id) override;
    void WritePage(Page& page,page_id_t page_id) override;
    void Clear() override;
  };
}
