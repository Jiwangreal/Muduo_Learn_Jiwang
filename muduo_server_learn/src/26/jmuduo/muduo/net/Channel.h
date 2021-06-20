// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.Channel不拥有fd，其关闭的时候，不用close关闭fd；
/// The file descriptor could be a socket,FileDescriptor是被Socket所拥有
/// 可能是an eventfd, a timerfd, or a signalfd，socket fd，总而言之是文件描述符
class Channel : boost::noncopyable
{
 public:
  typedef boost::function<void()> EventCallback;//时间的回调处理
  typedef boost::function<void(Timestamp)> ReadEventCallback;//读事件的回调处理，多了个时间戳

  //一个EventLoop包含多个Channel，一个Channel只能由一个EventLoop负责，所属的EventLoop只有1个
  //它只能在一个EventLoop线程中处理，所以会记录所属的EventLoop：EventLoop* loop_;			// 所属EventLoop
  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);

  //回调函数的注册，读回调
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }

  //写回调
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }
  
  //关闭回调
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }
  
  //错误回调函数
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

  //Channel所对应的fd
  int fd() const { return fd_; }

  //注册了哪些事件，保存在events_
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; } // used by pollers
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  //关注：events_事件或者可读事件kReadEvent，
  //pdate()会导致调用loop_->updateChannel(this);将Channel注册进来，实际上调用了Poller->updateChannel()
  //所以，相当于关注它的可读事件，把Channel注册到EventLoop，从而注册到EventLoop所持有的Poller对象中
  void enableReading() { events_ |= kReadEvent; update(); }
  // void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }

  //不关注事件了
  void disableAll() { events_ = kNoneEvent; update(); }
  bool isWriting() const { return events_ & kWriteEvent; }

  // for Poller
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // for debug，将事件转成字符串以便调试
  string reventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;			// 所属EventLoop
  const int  fd_;			// 文件描述符，但不负责关闭该文件描述符
  int        events_;		// 关注的事件
  int        revents_;		// poll/epoll返回的事件
  int        index_;		// used by Poller.表示在poll的事件数组struct pollfd中的序号，若<0说明是新增的一个事件，那么就添加到它的末尾
  bool       logHup_;		// for POLLHUP，负责生存期控制

  boost::weak_ptr<void> tie_;
  bool tied_;
  bool eventHandling_;		// 是否处于处理事件中
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
