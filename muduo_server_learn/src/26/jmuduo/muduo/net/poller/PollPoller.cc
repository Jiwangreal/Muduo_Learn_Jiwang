// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/PollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Channel.h>

#include <assert.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller()
{
}

//这里假设poll里面已经有一些通道了。通道注册是在：updateChannel
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change
  //pollfds_是向量vector，迭代器pollfds_.begin()，*pollfds_.begin()迭代器里面的元素
  //&*pollfds_.begin()数组的首地址
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
  Timestamp now(Timestamp::now());

  //>0说明返回了一些事件
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);//将事件放到activeChannels
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "PollPoller::poll()";
  }
  return now;
}

void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
  //pollfds_是输入输出参数，有数据的话，里面的内容会改动，遍历向量pollfds_
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    //>0说明产生了事件
    if (pfd->revents > 0)
    {
      --numEvents;//处理一个就--
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);
      assert(ch != channels_.end());//断言通道不存在，因为前面通道updateChannel过，所以这里肯定是存在的
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);//将channel的pfd->revents返回回去
      // pfd->revents = 0;
      activeChannels->push_back(channel);//将channel pushback到activeChannels
                                          //以便于pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);进行处理
    }
  }
}

//注册或者更新通道
void PollPoller::updateChannel(Channel* channel)
{
  //必须在EventLoop所属线程中调用才可以
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

  //这里还并不知道这个通道在数组pollfds_.begin()中的所注册的位置还不知道，此时值是<0的（Channel构造函数初始化其值=-1），说明是一个新的通道
  if (channel->index() < 0)
  {
	// index < 0说明是一个新的通道
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());//找不到

    //准备一个struct pollfd
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;//返回的事件先=0
    pollfds_.push_back(pfd);//放到末尾
    int idx = static_cast<int>(pollfds_.size())-1;//序号=大小-1
    channel->set_index(idx);//说明这是已经存在的通道了，已经存在的通道的idx！=-1
    channels_[pfd.fd] = channel;//key是fd，value是channel
  }
  else
  {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());//能找到
    assert(channels_[channel->fd()] == channel);

    int idx = channel->index();//取出下标
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));//vector向量看成数组，pollfds_.size()表示数组的大小
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);//assert为啥这么写，见下面临时不关注的逻辑
    pfd.events = static_cast<short>(channel->events());//将channel->events()进行转化
    pfd.revents = 0;
	// 将一个通道暂时更改为不关注事件，但不从Poller中移除该通道
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
	  // 暂时忽略该文件描述符的事件
	  // 实际上这里pfd.fd 可以直接设置为-1，他不是合法的fd，表示我不管他它的事件了，但是
    //又没有从struct pollfd这里移除，这里仅是临时性的不关注它的事件，可能下一次还会更新它
      pfd.fd = -channel->fd()-1;	// 这样子设置是为了removeChannel优化
                                  //这里的含义是-channel->fd()：channel->fd()表示取反，加入channel->fd()=0，那么还是可以设置为-1.
                                  //同样直接pfd.fd = -1;也行
    }
  }
}

//真正从数组pollfds_.begin()中移除，下次poll的时候不关注了
void PollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  
  //要移除首先得能找到它
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  //移除的通道必须是没有事件了，若还在关注事件，我们是不能移除的，所以调用removeChannel()前，先要调用updateChannel()，更新为kNoneEvent
  assert(channel->isNoneEvent());

  //取出在数目中的下标位置
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

  //取出pfd，因为前面assert(channel->isNoneEvent());，所以：pfd.fd 一定= -channel->fd()-1
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());

  //用key来移除，返回是一个整数，一定是等于1；
  //用迭代器移除的话，要先find，再erase，因为用迭代器移除的话，返回的是下一个迭代器
  size_t n = channels_.erase(channel->fd());
  assert(n == 1); (void)n;

  //说明是最后一个
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1)
  {
    pollfds_.pop_back();
  }
  else
  {
    //若移除的元素是中间任意的那个，则将中间元素与最后一个元素先做交换，然后将最后一个元素移除即可
	  // 这里移除的算法复杂度是O(1)，将待删除元素与最后一个元素交换再pop_back
    int channelAtEnd = pollfds_.back().fd;//channelAtEnd表示最后一个fd
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);//与最后一个进行交换

    /*
    fd=3,当updateChannel时，fd=-3-1=-4，-4对应的应该是3，所以下面的运算就是表示该意思：-(-4)-1=3
    */
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
    //更新最后一个元素的index：因为以前在数组的末尾，现在放到中间的某一位置了，所以需要更新index
    //假如以前末尾的位置是7，与位置4的元素进行了交换，那么末尾元素当前的位置是4
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

