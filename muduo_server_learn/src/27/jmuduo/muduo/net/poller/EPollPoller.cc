// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/EPollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>

using namespace muduo;
using namespace muduo::net;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

namespace
{
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}

EPollPoller::EPollPoller(EventLoop* loop)
  : Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)//预先分配能够容纳的事件个数
{
  if (epollfd_ < 0)
  {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EPollPoller::~EPollPoller()
{
  ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  //epoll_wait返回的事件，保存在events_事件列表中
  int numEvents = ::epoll_wait(epollfd_,
                               &*events_.begin(),
                               static_cast<int>(events_.size()),
                               timeoutMs);
  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
    if (implicit_cast<size_t>(numEvents) == events_.size())//达到该值，才增长
    {
      //刚开始是16，后面就增长32，64..
      //随诊你所关注的事件越多，则后面能够并发返回的事件个数就越多
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "EPollPoller::poll()";
  }
  return now;
}

//把返回到的事件numEvents添加到activeChannels
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i)
  {
    //返回实际的channel
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
    //调试用的
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    //把返回的事件写到channel中
    channel->set_revents(events_[i].events);
    //返回到EventLoop中，EventLoop取遍历这些通道来进行处理
    activeChannels->push_back(channel);
  }
}

void EPollPoller::updateChannel(Channel* channel)
{
  //断言处在I/O线程中
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  const int index = channel->index();

  //表明是新的通道，poll的index是下标序号，epoll的index是状态
  if (index == kNew || index == kDeleted)
  {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew)
    {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;//将通道channel添加到map中
    }
    else // index == kDeleted
    {
      //首先 channel->set_index(kDeleted);，但是若将channel再次放到epoll关注的话
      assert(channels_.find(fd) != channels_.end());//断言通道已经存在了
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded);//表示当前通道的状态处于已添加的状态
    update(EPOLL_CTL_ADD, channel);//将其添加到epoll中
  }
  else
  {
    //若将其更改为写的事件或者将其从epoll中剔除
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());//断言能找到
    assert(channels_[fd] == channel);//map里面key是fd，value是channel
    assert(index == kAdded);
    if (channel->isNoneEvent())//剔除的epoll的一个关注设置为没有事件
    {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);//状态是kDeleted，仅仅代表是不再epoll中关注，并不代表从channels_中移除了
    }
    else
    {
      update(EPOLL_CTL_MOD, channel);//状态不变
    }
  }
}

void EPollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  int fd = channel->fd();
  LOG_TRACE << "fd = " << fd;
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());

  int index = channel->index();
  assert(index == kAdded || index == kDeleted);//不能等于kNew，因为等于kNew说明根本就不在通道当中

  //从channels_中移除
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  //从epoll关注中移除
  if (index == kAdded)
  {
    update(EPOLL_CTL_DEL, channel);
  }

  
  channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel)
{
  //准备一个struct epoll_event 
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();//关注的事件是：channel->events()
  //ptr指针指向channel通道，若产生了event事件，同时也能将通道带回来，因为ptr指针指向了通道
  //目的是 Channel* channel = static_cast<Channel*>(events_[i].data.ptr);知道哪个通道产生了事件
  event.data.ptr = channel;

  int fd = channel->fd();
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
  {
    if (operation == EPOLL_CTL_DEL)
    {
      LOG_SYSERR << "epoll_ctl op=" << operation << " fd=" << fd;
    }
    else
    {
      //若是EPOLL_CTL_ADD失败，则该程序就不能进行了
      LOG_SYSFATAL << "epoll_ctl op=" << operation << " fd=" << fd;
    }
  }
}

