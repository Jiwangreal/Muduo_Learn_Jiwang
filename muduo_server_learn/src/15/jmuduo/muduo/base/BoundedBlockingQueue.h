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
  //���еĴ�СmaxSize
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

  //������Ʒ
  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    //������Ʒ���ж϶����Ƿ����ˣ�������wait
    while (queue_.full())
    {
      notFull_.wait();
    }
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify(); // TODO: move outside of lock
  }

  //���Ѳ�Ʒ
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
    //����һ����Ʒ�����¶��в���������֪ͨ
    notFull_.notify(); // TODO: move outside of lock
    return front;
  }
  
  //�ж϶����Ƿ�Ϊ��
  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  //�ж϶����Ƿ�Ϊ��
  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

  //�ж϶����е�ǰ��Ʒ�ĸ���
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  //����������
  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }

 private:
  mutable MutexLock          mutex_;
  Condition                  notEmpty_;
  Condition                  notFull_;//����ж��Ƿ���
  boost::circular_buffer<T>  queue_;//�����н磬��ʹ�ø����λ�����������Ļ��λ�������boost�ģ��������Ʒ
                                    //Linux����������%�����鷽ʽʵ���˻��λ�����
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
