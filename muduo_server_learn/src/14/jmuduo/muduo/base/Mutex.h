// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include <muduo/base/CurrentThread.h>
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{
//MutexLock类继承至noncopyable，表示不可以拷贝的
class MutexLock : boost::noncopyable
{
 public:
  MutexLock()
    : holder_(0)//holder_=0表示这个锁没有被任何一个线程使用
  {
    int ret = pthread_mutex_init(&mutex_, NULL);
    assert(ret == 0); (void) ret;
  }

  ~MutexLock()
  {
    assert(holder_ == 0);//断言这个锁没有被任何一个线程使用，才会去销毁这个锁
    int ret = pthread_mutex_destroy(&mutex_);
    assert(ret == 0); (void) ret;
  }

  bool isLockedByThisThread()
  {
    return holder_ == CurrentThread::tid();
  }

//断言当前线程是否有锁
  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock()
  {
    pthread_mutex_lock(&mutex_);
    holder_ = CurrentThread::tid();//将当前线程的tid保存至holder_
  }

  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:

  pthread_mutex_t mutex_;
  pid_t holder_;
};

class MutexLockGuard : boost::noncopyable
{
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();//构造函数中调用lock
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();//析构函数中调用unlock
  }

 private:

  MutexLock& mutex_;//MutexLockGuard对象生命期结束的时候，mutex_对象的生命期未结束
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);//不运行构造一个匿名的mutex对象，要不然会报错
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
