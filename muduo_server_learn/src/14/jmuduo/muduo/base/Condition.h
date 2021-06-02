// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

class Condition : boost::noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    pthread_cond_init(&pcond_, NULL);//初始化条件变量
  }

  ~Condition()
  {
    pthread_cond_destroy(&pcond_);//销毁条件变量
  }

  //等待函数
  void wait()
  {
    pthread_cond_wait(&pcond_, mutex_.getPthreadMutex());
  }

  //等待一定的时间
  // returns true if time out, false otherwise.
  bool waitForSeconds(int seconds);

  void notify()
  {
    pthread_cond_signal(&pcond_);
  }

  void notifyAll()
  {
    pthread_cond_broadcast(&pcond_);
  }

 private:
  MutexLock& mutex_;//与MutexLock配合使用，不负责管理MutexLock他的生存期
  pthread_cond_t pcond_;
};

}
#endif  // MUDUO_BASE_CONDITION_H
