// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace muduo
{

//该线程池的线程数目是固定的，Linux网络编程实现了一个可以伸缩的线程池
class ThreadPool : boost::noncopyable
{
 public:
 //boost::function<void ()>是一个函数用来执行任务
  typedef boost::function<void ()> Task;

  explicit ThreadPool(const string& name = string());
  ~ThreadPool();

  //启动线程池，启动的线程个数固定
  void start(int numThreads);
  //关闭线程池
  void stop();

  //运行任务，往线程池的任务队列添加任务
  void run(const Task& f);

 private:
 //线程池中的线程要执行的函数
  void runInThread();
  //线程池中的线程需要获取任务，去执行任务
  Task take();

  MutexLock mutex_;
  Condition cond_;//用来唤醒线程来执行任务
  string name_;//线程名称
  boost::ptr_vector<muduo::Thread> threads_;//存放线程指针
  std::deque<Task> queue_;//使用stl的deque来实现。内部的数据元素类型是Task
  bool running_;//表示线程池是否运行
};

}

#endif
