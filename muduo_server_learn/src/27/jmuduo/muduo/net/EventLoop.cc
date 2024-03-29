// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>

//#include <poll.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
// 当前线程EventLoop对象指针
// 线程局部存储
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;//10s超时
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    threadId_(CurrentThread::tid()),
	poller_(Poller::newDefaultPoller(this)),//构造Poller对象
	currentActiveChannel_(NULL)
{
  LOG_TRACE << "EventLoop created " << this << " in thread " << threadId_;
  // 如果当前线程已经创建了EventLoop对象，终止(LOG_FATAL)
  if (t_loopInThisThread)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
}

EventLoop::~EventLoop()
{
  t_loopInThisThread = NULL;
}

// 事件循环，该函数不能跨线程调用
// 只能在创建该对象的线程中调用
void EventLoop::loop()
{
  assert(!looping_);
  // 断言当前处于创建该对象的线程中
  assertInLoopThread();
  looping_ = true;
  LOG_TRACE << "EventLoop " << this << " start looping";

  //::poll(NULL, 0, 5*1000);
  //while里面的内容还是处于I/O线程在调用
  while (!quit_)
  {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);//activeChannels_返回
    //++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    eventHandling_ = true;

    //遍历activeChannels_进行处理
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    //doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

// 该函数可以跨线程调用，不一定总是在I/O线程中调用的
/*
若在其他线程中调用quit()，假设I/O线程阻塞在poller_->poll()位置或者currentActiveChannel_->handleEvent(pollReturnTime_);这个位置，
我们需要唤醒它，否则loop()中没有机会运行到while (!quit_)这里。
需要唤醒wakeup()它：
（1）方法1：
可以使用pip来实现，poller_->poll()可以多一个管道fd，监听其可读事件；
wakeup()向管道中写入一个字符，poller_->poll()可读了，返回一个管道的通道activeChannels_

（2）方法2：eventfd
*/
void EventLoop::quit()
{
  //由于是跨线程调用，quit_不需要保护，因为Linux底下bool类型是原子操作、整型变量的操作则需要原子性操作
  quit_ = true;//使得 while (!quit_)循环结束
  if (!isInLoopThread())
  {
    //不是在I/O线程中调用quit()，还需要调用wakeup()
    //wakeup();
  }
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}
