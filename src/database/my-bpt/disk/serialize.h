// In serialize.h

#pragma once

#include <cstring>
#include <memory>
#include <concepts> // Include for concepts
// #include "IO_utils.h" // Avoid circular include if possible, maybe forward declare?
#include "common.h" // Assuming PAGESIZE and page_id_t are here

namespace RFlowey {
  template <typename T>
  concept PageAble = std::is_trivially_copyable_v<T> && requires {
    sizeof(T) <= PAGESIZE;
  };

  template<typename T>
    requires PageAble<T>
  void Serialize(void* _dest, const T& value) {
    std::memcpy(_dest, &value, sizeof(T));
  }

  template<typename T>
    requires PageAble<T>
  std::unique_ptr<T> Deserialize(const void* _src) {//copy a object from the data;
    return std::make_unique<T>(*reinterpret_cast<const T*>(_src));
  }

}