// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());
  EventLoop* getNextLoop();

 private:

  EventLoop* baseLoop_;	// 与Acceptor所属EventLoop相同
  bool started_;//是否启动
  int numThreads_;		// 线程数
  int next_;			// 新连接到来，所选择的EventLoop对象下标
                  //选择一个线程来处理
  boost::ptr_vector<EventLoopThread> threads_;		//维护了一个IO线程列表，使用ptr_vector来管理
                                                  //当ptr_vector对象销毁的时候，它所管理的对象EventLoopThread也会跟着销毁
  std::vector<EventLoop*> loops_;					// EventLoop列表
                                          //一个IO线程对应一个EventLoop对象，这些对象都是栈上对象，不需要我们来销毁，因而使用vector即可
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
