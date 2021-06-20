// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

class EventLoop;

class EventLoopThread : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  // ThreadInitCallback()默认是空的回调函数
  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback());
  ~EventLoopThread();
  EventLoop* startLoop();	// 启动线程thread_.start();，该线程就成为了IO线程
                          //在startLoop()内部会创建一个EventLoop对象，将 loop_指针指向一个EventLoop对象

 private:
  void threadFunc();		// 线程函数

  EventLoop* loop_;			// loop_指针指向一个EventLoop对象，一个IO线程有且只有一个EventLoop对象
  bool exiting_;//是否退出
  Thread thread_;//基于对象编程是包含一个Thread类对象，若是面向对象编程则应该是继承Thread类对象
  
  //配合使用
  MutexLock mutex_;
  Condition cond_;
  
  ThreadInitCallback callback_;		// 若回调函数不是空的，则回调函数在EventLoop::loop事件循环之前被调用
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

