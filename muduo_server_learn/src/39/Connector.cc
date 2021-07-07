// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Connector.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

// 可以跨线程调用
void Connector::start()
{
  connect_ = true;
  // 在lopp_所对应的IO线程中执行startInLoop
  loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

void Connector::startInLoop()
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

// 可以跨线程调用
// 另外一个线程调用stop()，使得connect_置为false
void Connector::stop()
{
  connect_ = false;
  loop_->runInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
  // FIXME: cancel timer
}

void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  // kConnecting表示正处于连接的状态，还没有连接成功
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();	// 将通道从poller中移除关注，并将channel置空
    retry(sockfd);		// 这里并非要重连，只是调用sockets::close(sockfd);因为此时connect_ = false;不可能再发起连接了
  }
}

void Connector::connect()
{
  int sockfd = sockets::createNonblockingOrDie();	// 创建非阻塞套接字

  // 调用connect()发起连接
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:	// 非阻塞套接字，未连接成功返回码是EINPROGRESS表示正在连接
    case EINTR:
    case EISCONN:			// 连接成功
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);		// 重连
      break;            //表示重连，没有连接成功

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);	// 不能重连，关闭sockfd
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}

// 不能跨线程调用
void Connector::restart()
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;//设置初始的重连时间
  connect_ = true;
  startInLoop();
}

void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);
  // 创建Channel对象，Channel与sockfd关联
  channel_.reset(new Channel(loop_, sockfd));

  // 设置可写回调函数，这时候如果socket没有错误，sockfd就处于可写状态
  // 一旦连接成功就会回调handleWrite，它就处于可写状态
  channel_->setWriteCallback(
      boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe

  // 设置错误回调函数
  channel_->setErrorCallback(
      boost::bind(&Connector::handleError, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  channel_->enableWriting();		// 让Poller关注可写事件
}

int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();			// 从poller移除关注
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  
  // 不能在这里重置channel_，因为正在调用Channel::handleEvent，从而调用了handleWrite()，从而调用了removeAndResetChannel()
  // 若此时重置channel_,此时调用channel_的函数还没有返回。所以重置channel_不能在removeAndResetChannel()这个函数中执行
  // 将resetChannel加入到IO线程的队列中，保证跳出这个removeAndResetChannel()函数再执行
  loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

void Connector::resetChannel()
{
  channel_.reset();		// channel_ 置空
}


void Connector::handleWrite()
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
    // 连接成功了，该channel_就没有用了，就不需要关注他的可写事件了
    int sockfd = removeAndResetChannel();	// 从poller中移除关注，并将channel置空
    // socket可写并不意味着连接一定建立成功
    // 还需要用getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)再次确认一下。
    int err = sockets::getSocketError(sockfd);
    if (err)		// 有错误
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd);		// 重连
    }
    else if (sockets::isSelfConnect(sockfd))		// 自连接
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd);		// 重连
    }
    else	// 连接成功
    {
      setState(kConnected);
      if (connect_)
      {
        newConnectionCallback_(sockfd);		// 回调
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError";
  assert(state_ == kConnecting);

  int sockfd = removeAndResetChannel();		// 将channel从poller中移除关注，并将channel置空
  int err = sockets::getSocketError(sockfd);
  LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
  retry(sockfd);
}

// 采用back-off策略（退避策略）重连，即重连时间逐渐延长，0.5s, 1s, 2s, ...直至30s
void Connector::retry(int sockfd)
{
  sockets::close(sockfd);
  setState(kDisconnected);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";
    // 注册一个定时操作，返回一个定时器，重连
    // retryDelayMs_/1000.0=毫秒/1000=秒
    // startInLoop再次发起重连
    loop_->runAfter(retryDelayMs_/1000.0,
                    boost::bind(&Connector::startInLoop, shared_from_this()));
    // 下一次的重连时间retryDelayMs_
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

