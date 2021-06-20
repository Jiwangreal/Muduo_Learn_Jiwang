// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this)),//this：将自身对象传递到threadFunc
    mutex_(),
    cond_(mutex_),//初始化条件变量，条件变量总是和一个互斥量配置使用
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit();		// 退出IO线程，让IO线程的loop循环退出，从而退出了IO线程
  thread_.join();
}

// 启动startLoop()，该线程就成为了IO线程
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());//看下是否启动

  thread_.start();//启动会调用线程的回调函数，这里bind的是threadFunc
                  //所以此时就有2个线程，一个是startLoop()线程，一个是threadFunc()线程，并发执行

  {
    MutexLockGuard lock(mutex_);
    // loop_不为空，意味着threadFunc()已经运行完毕，因为这里要返回EventLoop对象指针，EventLoop*
    while (loop_ == NULL)
    {
      cond_.wait();
    }
  }

  return loop_;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);//若有callback_，则先初始化一下
  }

  {
    MutexLockGuard lock(mutex_);
    // loop_指针指向了一个栈上的对象，threadFunc函数退出之后，这个指针就失效了
    // 因为threadFunc函数退出，就意味着线程退出了，EventLoopThread对象也就没有存在的价值了，应该delete掉EventLoopThread对象
    // 因而不会有什么大的问题
    // muduo对EventLoopThread封装并没有实现未自动销毁型的，这里对对象销毁做了简化，因为整个程序都结束了，是否销毁是没有关系的
    // 因为muduo的线程池中的线程数是固定的，开始执行时就创建好了，这些线程池与整个程序的生命期是一样的，
    //也就是说下面的loop.loop();退出了，就意味着整个程序退出了,所以是否销毁EventLoopThread对象也没什么关系，也不可能通过loop_指针访问threadFunc()对象了
    // 虽然它失效了，因为threadFunc()线程结束了，就意味着整个程序结束了
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  //assert(exiting_);
}

