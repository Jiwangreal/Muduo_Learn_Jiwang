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

  //判断当前线程是否拥有锁
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
  //加锁
  void lock()
  {
    pthread_mutex_lock(&mutex_);
    holder_ = CurrentThread::tid();//将当前线程的tid保存至holder_
  }

  //解锁
  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  //获取pthread_mutex_t对象
  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:

  pthread_mutex_t mutex_;//保存pthread_mutex_t对象
  pid_t holder_;//当前拥有该锁的线程ID，线程真实tid
};

/*
MutexLockGuard类，此类更加常用，使用RAII技法封装

void f()
{
	mutex.lock()
	...
	if(条件)
	{
		return;//这里直接return，如果忘记umutex.unlock，会造成死锁
	}
	mutex.unlock;
}


void f()
{
	MutexLockGuard lock(mutex);//在构造函数中加锁了
	...
	if(条件)
	{
		return;//这里直接return，那么lock对象的生命周期就结束了，自动调用析构函数解锁
	}
	
}//这里返回，和上面的return解释一样

*/
class MutexLockGuard : boost::noncopyable
{
 public:
 //构造函数中获取资源并初始化
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

  MutexLock& mutex_;//这里的引用说明，MutexLockGuard类并不管理mutex_对象的生存期
                    //MutexLockGuard对象生命期结束的时候，mutex_对象的生命期未结束
                    //这俩类是关联关系：MutexLockGuard类使用了mutex_中lock()方法和unlock()方法，不存在整体与局部的关系
  /*
  类之间的关系
（1）关联关系
MutexLockGuard类使用了mutex类中的lock和unlock方法
（2）聚合关系
若存在整体与局部的关系，则是聚合关系
（3）组合关系
不仅存在整体与局部的关系，以及还负责对对象的销毁，就是组合关系
  
  */

};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);//不运行构造一个匿名的mutex对象，要不然会报错
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name" //目的为了防止错误的用法，编译会不通过

#endif  // MUDUO_BASE_MUTEX_H
