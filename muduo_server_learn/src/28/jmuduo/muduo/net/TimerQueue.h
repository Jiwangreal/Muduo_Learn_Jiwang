// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Channel.h>

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
//定时器队列，可以看成是定时器的管理器，内部维护了一个定时器列表
class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);//一个TimerQueue属于一个EventLoop对象，EventLoop对象是在某个I/O线程中创建的
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  // 一定是线程安全的，可以跨线程调用。通常情况下被其它线程调用。可以不再所属的EventLoop的I/O线程中调用
  //添加定时器，插入到定时器列表中
  //外部类，返回一个TimerId
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  //也可以跨线程调用
  //取消一个定时器
  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // unique_ptr是C++ 11标准的一个独享所有权的智能指针
  // 无法得到指向同一对象的两个unique_ptr指针
  // 但可以进行移动构造与移动赋值操作，即所有权可以移动到另一个对象（而非拷贝构造），这里涉及到右值引用的问题
  //使用unique_ptr，则不需要手动调用delete
  typedef std::pair<Timestamp, Timer*> Entry;//时间戳和定时器的地址
  typedef std::set<Entry> TimerList;//key是Entry，TimerList按照Timestamp来排序
  typedef std::pair<Timer*, int64_t> ActiveTimer;//地址和序号
  typedef std::set<ActiveTimer> ActiveTimerSet;//按照地址Timer*排序
  //ActiveTimerSet和TimerList保存的是相同的数据，都是定时器列表，但是排序不同而已

  // 以下成员函数只可能在其所属的I/O线程中调用，因而不必加锁。
  // 服务器性能杀手之一是锁竞争，所以要尽可能少用锁
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers
  // 返回超时的定时器列表
  std::vector<Entry> getExpired(Timestamp now);

  //对超时的定时器进行重置，因为超时的定时器可能是重复的定时器
  void reset(const std::vector<Entry>& expired, Timestamp now);

  //插入定时器
  bool insert(Timer* timer);

  EventLoop* loop_;		// 所属EventLoop
  const int timerfd_;//timerfd_create 所创建的定时器fd
  Channel timerfdChannel_;//定时器通道，当定时器事件到来的时候，可读事件产生会回调handleRead()
  // Timer list sorted by expiration
  TimerList timers_;	// timers_是按到期时间排序的定时器列表

  // for cancel()
  // timers_与activeTimers_保存的是相同的数据
  // timers_是按到期时间排序，activeTimers_是按对象地址排序
  ActiveTimerSet activeTimers_;
  bool callingExpiredTimers_; /* atomic *///调用是否处于处理超时定时器当中
  ActiveTimerSet cancelingTimers_;	// 保存的是被取消的定时器
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
