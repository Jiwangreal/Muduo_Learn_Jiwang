// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <deque>
#include <assert.h>

namespace muduo
{

template<typename T>
class BlockingQueue : boost::noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  //生产产品
  void put(const T& x)
  {
    //生产产品对队列进行mutex保护
    MutexLockGuard lock(mutex_);//构造函数中lock，析构函数中unlock
    queue_.push_back(x);//生产产品
    //通知消费者线程去消费
    notEmpty_.notify(); // TODO: move outside of lock
    /*
    mutex保护的范围可以小一些
    {
        MutexLockGuard lock(mutex_);
        queue_.push_back(x);
    }
    notEmpty_.notify(); // TODO: move outside of lock
    */
  }

  //消费产品
  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait();//队列不为空，则跳出
    }
    assert(!queue_.empty());
    T front(queue_.front());//取出第一个元素
    queue_.pop_front();//弹出第一个元素
    return front;
  }

  //大小
  //const 成员函数是不可以去改变非const成员的，但是mutex_是mutable的，那么就是可以改变的了
  size_t size() const
  {
    //可能多个线程访问队列的大小，所以也需要保护
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
 //mutable表示可变的
  mutable MutexLock mutex_;//条件变量一般要与互斥量配合使用
  Condition         notEmpty_;//只用了一个条件变量
  std::deque<T>     queue_;
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
