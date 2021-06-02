// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include "muduo/base/noncopyable.h"

#include <stdint.h>

namespace muduo
{

namespace detail
{
template<typename T>//T表示传递进来的类型
class AtomicIntegerT : noncopyable //表示AtomicIntegerT类型是不可以拷贝的，就是将=运算符做成私有的
{
 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  // uncomment if you need copying and assignment
  //
  // AtomicIntegerT(const AtomicIntegerT& that)
  //   : value_(that.get())
  // {}
  //
  // AtomicIntegerT& operator=(const AtomicIntegerT& that)
  // {
  //   getAndSet(that.get());
  //   return *this;
  // }

  T get()
  {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    //先比较在设置：若value==0，就将value的值设置为0，并返回value的值；若不相等，则直接返回value
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

  T getAndAdd(T x)
  {
    // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
    //先获取再加，返回的是没有修改过的value的值，再加x
    return __sync_fetch_and_add(&value_, x);
  }

  T addAndGet(T x)
  {
    //先加后获取
    return getAndAdd(x) + x;
  }

  T incrementAndGet()
  {
    //自增，先加后获取
    return addAndGet(1);
  }

  T decrementAndGet()
  {
    //自减，先减后获取
    return addAndGet(-1);
  }

  void add(T x)
  {
    getAndAdd(x);
  }

  void increment()
  {
    incrementAndGet();
  }

  void decrement()
  {
    decrementAndGet();
  }

  T getAndSet(T newValue)
  {
    // in gcc >= 4.7: __atomic_exchange_n(&value, newValue, __ATOMIC_SEQ_CST)
    //返回原来的值，并设置其为新值
    return __sync_lock_test_and_set(&value_, newValue);
  }

 private:
  volatile T value_;
};
}  // namespace detail

//模板实例化
typedef detail::AtomicIntegerT<int32_t> AtomicInt32;//32bit的原子性整数类
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

}  // namespace muduo

#endif  // MUDUO_BASE_ATOMIC_H
