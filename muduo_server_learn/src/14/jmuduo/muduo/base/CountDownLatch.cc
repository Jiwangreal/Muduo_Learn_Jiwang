// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),
    condition_(mutex_),
    count_(count)
{
}

void CountDownLatch::wait()
{
  //条件变量的使用规范，等待count_=0
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();
  }
}

void CountDownLatch::countDown()
{
  MutexLockGuard lock(mutex_);
  --count_;
  if (count_ == 0) {
    condition_.notifyAll();//通知所有的等待线程
  }
}

//const成员函数不能改变count_成员变量的状态
//而在这里却能改变mutex_状态，是因为他的类型是mutable
int CountDownLatch::getCount() const
{
  MutexLockGuard lock(mutex_);
  return count_;
}

