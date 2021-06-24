// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThreadPool.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
  : baseLoop_(baseLoop),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

// 启动线程池
void EventLoopThreadPool::start(const ThreadInitCallback& cb)// 启动EventLoopThread线程，在进入事件循环之前，会调用cb
{                                                             //EventLoopThread中已讲过
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  // 创建若干个线程
  for (int i = 0; i < numThreads_; ++i)
  {
    EventLoopThread* t = new EventLoopThread(cb);
    threads_.push_back(t);
    loops_.push_back(t->startLoop());	// 启动EventLoopThread线程，在进入事件循环之前，会调用cb
                                      //启动EventLoopThread线程
  }
  //numThreads_ == 0说明没有创建新的IO线程
  if (numThreads_ == 0 && cb)
  {
    // 只有一个EventLoop，在这个EventLoop进入事件循环之前，调用cb
    cb(baseLoop_);
  }
}

// 当新的连接到来时，需要选择一个EventLoop对象来处理
EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_;//baseLoop_是acceptor所属的loop，也就是mainReactor

  // 如果loops_为空，则loop指向baseLoop_，说明只有一个EventLoop，也就是说只有一个mainReactor，那么mainReactor既要处理监听socket和已连接socket的事件
  // 如果不为空，按照round-robin（RR，轮叫）的调度方式选择一个EventLoop
  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_];
    ++next_;//下一个EventLoop的下标
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
}

