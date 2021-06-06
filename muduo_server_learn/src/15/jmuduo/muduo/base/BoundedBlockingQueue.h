// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/circular_buffer.hpp>
#include <boost/noncopyable.hpp>
#include <assert.h>

namespace muduo
{

template<typename T>
class BoundedBlockingQueue : boost::noncopyable
{
 public:
  //队列的大小maxSize
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

  //生产产品
  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    //生产产品先判断队列是否满了，满了则wait
    while (queue_.full())
    {
      notFull_.wait();
    }
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify(); // TODO: move outside of lock
  }

  //消费产品
  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
    //消费一个产品，导致队列不满，则发起通知
    notFull_.notify(); // TODO: move outside of lock
    return front;
  }
  
  //判断队列是否为空
  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  //判断队列是否为满
  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

  //判断队列中当前产品的个数
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  //队列总容量
  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }

 private:
  mutable MutexLock          mutex_;
  Condition                  notEmpty_;
  Condition                  notFull_;//多个判断是否满
  boost::circular_buffer<T>  queue_;//队列有界，则使用个环形缓冲区（这里的环形缓冲器是boost的）来保存产品
                                    //Linux网络编程用了%和数组方式实现了环形缓冲区
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
