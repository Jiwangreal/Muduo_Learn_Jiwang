// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());//断言线程池中线程的个数=0
  running_ = true;
  threads_.reserve(numThreads);//threads_是一个vector，预先准备这么多的空间
  
  //创建线程
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);
    //new Thread创建线程，将线程指针存放到threads_，创建的线程绑定的函数是runInThread，线程的名称：线程池的名称+id
    threads_.push_back(new muduo::Thread(
          boost::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i].start();//启动线程
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  cond_.notifyAll();//通知所有等待线程
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));//等待所有线程退出
}

void ThreadPool::run(const Task& task)
{
  //判断没有线程可运行，则直接运行当前的任务
  if (threads_.empty())
  {
    task();
  }
  else//否则添加进去
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task);
    cond_.notify();//条件变量通知
  }
}

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);//任务队列需要保护
  // always use a while-loop, due to spurious wakeup
  //若任务队列的任务是空的且处于运行
  while (queue_.empty() && running_)
  {
    cond_.wait();//等待任务
  }
  //一旦有任务到来
  Task task;
  if(!queue_.empty())
  {
    task = queue_.front();//取出任务
    queue_.pop_front();//并弹出该任务
  }
  return task;
}

//runInThread启动，实际上处于等待任务的状态cond_.wait();或者等待线程池结束的条件：running_=false
void ThreadPool::runInThread()
{
  try
  {
    while (running_)
    {
      Task task(take());
      if (task)
      {
        task();//执行任务
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

