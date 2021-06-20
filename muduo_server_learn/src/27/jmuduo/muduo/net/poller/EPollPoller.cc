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
    events_(kInitEventListSize)//Ԥ�ȷ����ܹ����ɵ��¼�����
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
  //epoll_wait���ص��¼���������events_�¼��б���
  int numEvents = ::epoll_wait(epollfd_,
                               &*events_.begin(),
                               static_cast<int>(events_.size()),
                               timeoutMs);
  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
    if (implicit_cast<size_t>(numEvents) == events_.size())//�ﵽ��ֵ��������
    {
      //�տ�ʼ��16�����������32��64..
      //����������ע���¼�Խ�࣬������ܹ��������ص��¼�������Խ��
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

//�ѷ��ص����¼�numEvents��ӵ�activeChannels
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i)
  {
    //����ʵ�ʵ�channel
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
    //�����õ�
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    //�ѷ��ص��¼�д��channel��
    channel->set_revents(events_[i].events);
    //���ص�EventLoop�У�EventLoopȡ������Щͨ�������д���
    activeChannels->push_back(channel);
  }
}

void EPollPoller::updateChannel(Channel* channel)
{
  //���Դ���I/O�߳���
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  const int index = channel->index();

  //�������µ�ͨ����poll��index���±���ţ�epoll��index��״̬
  if (index == kNew || index == kDeleted)
  {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew)
    {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;//��ͨ��channel��ӵ�map��
    }
    else // index == kDeleted
    {
      //���� channel->set_index(kDeleted);����������channel�ٴηŵ�epoll��ע�Ļ�
      assert(channels_.find(fd) != channels_.end());//����ͨ���Ѿ�������
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded);//��ʾ��ǰͨ����״̬��������ӵ�״̬
    update(EPOLL_CTL_ADD, channel);//������ӵ�epoll��
  }
  else
  {
    //���������Ϊд���¼����߽����epoll���޳�
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());//�������ҵ�
    assert(channels_[fd] == channel);//map����key��fd��value��channel
    assert(index == kAdded);
    if (channel->isNoneEvent())//�޳���epoll��һ����ע����Ϊû���¼�
    {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);//״̬��kDeleted�����������ǲ���epoll�й�ע�����������channels_���Ƴ���
    }
    else
    {
      update(EPOLL_CTL_MOD, channel);//״̬����
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
  assert(index == kAdded || index == kDeleted);//���ܵ���kNew����Ϊ����kNew˵�������Ͳ���ͨ������

  //��channels_���Ƴ�
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  //��epoll��ע���Ƴ�
  if (index == kAdded)
  {
    update(EPOLL_CTL_DEL, channel);
  }

  
  channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel)
{
  //׼��һ��struct epoll_event 
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();//��ע���¼��ǣ�channel->events()
  //ptrָ��ָ��channelͨ������������event�¼���ͬʱҲ�ܽ�ͨ������������Ϊptrָ��ָ����ͨ��
  //Ŀ���� Channel* channel = static_cast<Channel*>(events_[i].data.ptr);֪���ĸ�ͨ���������¼�
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
      //����EPOLL_CTL_ADDʧ�ܣ���ó���Ͳ��ܽ�����
      LOG_SYSFATAL << "epoll_ctl op=" << operation << " fd=" << fd;
    }
  }
}

