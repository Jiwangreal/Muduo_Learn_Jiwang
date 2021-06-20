// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>

namespace muduo
{
namespace net
{
///
/// Internal class for timer event.
///
//Timer类对定时操作进行了抽象，没有调用定时器的相关函数timerfd_create 等
class Timer : boost::noncopyable
{
 public:
  Timer(const TimerCallback& cb, Timestamp when, double interval)
    : callback_(cb),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),//interval> 0.0，repeat_=true说明是重复的定时器，重复的定时器可以restart()
      sequence_(s_numCreated_.incrementAndGet())//incrementAndGet()先加后获取
                                                //创建的第一个定时器对象的序号是1
                                                //由于是原子操作，即使是多线程同时创建Timer对象，也能保证sequence_唯一
  { }

  void run() const
  {
    callback_();
  }

  Timestamp expiration() const  { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }

  //重启
  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  const TimerCallback callback_;		// 定时器回调函数
  Timestamp expiration_;				// 下一次的超时时刻，当超时时刻到来的时候，定时器回调函数被调用
  const double interval_;				// 超时时间间隔，如果是一次性定时器，该值为0
  const bool repeat_;					// 是否重复，若是false表示一次性定时器，若是true，表示重复定时器
  const int64_t sequence_;				// 定时器序号，每个定时器都有一个唯一的编号

  static AtomicInt64 s_numCreated_;		// 定时器计数，当前已经创建的定时器数量
                                      //类型是AtomicInt64原子操作类
};
}
}
#endif  // MUDUO_NET_TIMER_H
