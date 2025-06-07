#ifndef QUEUE_H
#define QUEUE_H

#include "stlite/exceptions.hpp"
namespace sjtu {
  template<typename T,size_t MAX_SIZE>
  struct queue {
    T data[MAX_SIZE]{};
    int _front=0;
    int _back=0;

    static int prev(int n) {
      return n-1<0?n-1+MAX_SIZE:n-1;
    }
    static int next(int n) {
      return n+1>=MAX_SIZE?n+1-MAX_SIZE:n+1;
    }

    T& front() {
      if(empty()) {
        throw;
      }
      return data[_front];
    }
    const T& front() const{
      if(empty()) {
        throw;
      }
      return data[_front];
    }
    T& back() {
      if(empty()) {
        throw;
      }
      return data[prev(_back)];
    }
    const T& back() const{
      if(empty()) {
        throw;
      }
      return data[prev(_back)];
    }

    void push_back(const T& value) {
      data[_back] = value;
      _back = next(_back);
    }
    void pop_back() {
      if(empty()) {
        throw;
      }
      _back = prev(_back);
    }

    void push_front(const T& value) {
      _front = prev(_front);
      data[_front] = value;
    }
    void pop_front() {
      if(empty()) {
        throw;
      }
      _front = next(_front);
    }
    [[nodiscard]] bool empty() const {
      return _back==_front;
    }
    size_t size() const {
      long long temp = _back-_front;
      if(temp<0) {
        temp+=MAX_SIZE;
      }
      return temp;
    }
  };
}
#endif //QUEUE_H
