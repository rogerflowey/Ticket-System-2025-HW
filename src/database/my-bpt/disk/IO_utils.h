#ifndef IO_UTILS_H
#define IO_UTILS_H

#include <fstream>
#include <memory>

#include "serialize.h"
#include "IO_manager.h"
#include "common.h"


namespace RFlowey {
  class IOManager;
  /**
   * A wrapper of a Byte Page(Temporary solution for no Buffer Pool)
   * Ensure the life span covers the value of it
   */
  class Page {
    char data_[4096]={};
    page_id_t page_id_;
    IOManager* manager_;
  public:
    Page() = delete;
    Page(IOManager* manager,page_id_t page_id);
    ~Page();
    char* get_data();
    void flush();
  };
  bool open(std::fstream& file,std::string& filename);



  /**
   * @return true if new
   */
  inline bool open(std::fstream& file,const std::string& filename) {
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if(!file){
      file.clear();
      file.open(filename, std::ios::out | std::ios::binary);
      file.close();
      file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
      return true;
    }
    return false;
  }


  //不要问为啥不分开,问就是模板类


  //代表“解引用”后的内存中的对象，析构时会同时析构并重新序列化管理的对象
  template<typename T>
  class PageRef {
    std::shared_ptr<Page> page_;
    std::unique_ptr<T> t_ptr_;
    bool is_dirty = false;
  public:
    bool is_valid = true;
    PageRef() = default;
    PageRef(std::shared_ptr<Page> page,std::unique_ptr<T>&& t_ptr):page_(std::move(page)),t_ptr_(std::move(t_ptr)){};
    PageRef(PageRef&& ref) noexcept :page_(std::move(ref.page_)),t_ptr_(std::move(ref.t_ptr_)) {
      is_dirty = ref.is_dirty;
      is_valid = ref.is_valid;
      ref.is_dirty = false;
      ref.is_valid = false;
    };

    PageRef& operator=(PageRef&& ref)  noexcept {
      if(this==&ref) {
        return *this;
      }
      Drop();

      page_ = std::move(ref.page_);
      t_ptr_ = std::move(ref.t_ptr_);
      is_dirty = ref.is_dirty;
      is_valid = ref.is_valid;
      ref.is_dirty = false;
      ref.is_valid = false;

      return *this;
    }

    PageRef(const PageRef&) = delete;
    PageRef& operator=(const PageRef&) = delete;

    ~PageRef() {
      Drop();
    }
    void Drop() {
      if(!is_valid) {
        return;
      }
      if(is_dirty) {
        Serialize(page_->get_data(),*t_ptr_);
      }
      is_valid = false;

    }
    T* operator->() {
      is_dirty = true;
      return t_ptr_.get();
    }
    const T* operator->() const{
      return t_ptr_.get();
    }
    T& operator*() {
      is_dirty = true;
      return *t_ptr_;
    }
    const T& operator*() const{
      return *t_ptr_;
    }
  };
  template<typename T>
  class PagePtr {
  public:
    IOManager* manager_;
    page_id_t page_id_;

    explicit PagePtr(page_id_t page_id,IOManager* manager):page_id_(page_id),manager_(manager){}

    [[nodiscard]] page_id_t page_id() const {
      return page_id_;
    }

    //get an ref to an EXISTING object on the page;
    [[nodiscard]] PageRef<T> get_ref() const {
      std::shared_ptr<Page> page = manager_->ReadPage(page_id_);
      return PageRef{page,Deserialize<T>(page->get_data())};
    }

    template<typename... Args>
    PageRef<T> make_ref(Args ...args) const {
      return make_ref(std::make_unique<T>(std::forward<Args>(args)...));
    }
    PageRef<T> make_ref(std::unique_ptr<T> t_obj_ptr) const {
      auto page = std::make_shared<Page>(manager_, page_id_);
      Serialize(page->get_data(), *t_obj_ptr);
      page->flush();
      return {std::move(page), std::move(t_obj_ptr)};
    }
  };

  template<typename T>
  PagePtr<T> allocate(IOManager* manager) {
    return PagePtr<T>{manager->NewPage(),manager};
  }
}

#endif //IO_UTILS_H
