// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>

namespace muduo
{

class CountDownLatch : boost::noncopyable
{
 public:

  explicit CountDownLatch(int count);//等待计数器-1

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;//用mutable修饰的原因需要注意下
  Condition condition_;
  int count_;//计数器
};

}
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
