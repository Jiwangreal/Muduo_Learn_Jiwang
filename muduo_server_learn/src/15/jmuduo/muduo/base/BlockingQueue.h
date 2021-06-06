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

  //������Ʒ
  void put(const T& x)
  {
    //������Ʒ�Զ��н���mutex����
    MutexLockGuard lock(mutex_);//���캯����lock������������unlock
    queue_.push_back(x);//������Ʒ
    //֪ͨ�������߳�ȥ����
    notEmpty_.notify(); // TODO: move outside of lock
    /*
    mutex�����ķ�Χ����СһЩ
    {
        MutexLockGuard lock(mutex_);
        queue_.push_back(x);
    }
    notEmpty_.notify(); // TODO: move outside of lock
    */
  }

  //���Ѳ�Ʒ
  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait();//���в�Ϊ�գ�������
    }
    assert(!queue_.empty());
    T front(queue_.front());//ȡ����һ��Ԫ��
    queue_.pop_front();//������һ��Ԫ��
    return front;
  }

  //��С
  //const ��Ա�����ǲ�����ȥ�ı��const��Ա�ģ�����mutex_��mutable�ģ���ô���ǿ��Ըı����
  size_t size() const
  {
    //���ܶ���̷߳��ʶ��еĴ�С������Ҳ��Ҫ����
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
 //mutable��ʾ�ɱ��
  mutable MutexLock mutex_;//��������һ��Ҫ�뻥�������ʹ��
  Condition         notEmpty_;//ֻ����һ����������
  std::deque<T>     queue_;
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
