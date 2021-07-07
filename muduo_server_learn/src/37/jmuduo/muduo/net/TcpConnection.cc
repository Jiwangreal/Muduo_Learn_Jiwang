// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpConnection.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 默认连接到来的回调函数
void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

// 默认消息到来的回调函数
void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
  buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(CHECK_NOTNULL(loop)),
    name_(nameArg),
    state_(kConnecting),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr)/*,
    highWaterMark_(64*1024*1024)*/
{
  // 通道可读事件到来的时候，回调TcpConnection::handleRead，_1是事件发生时间
  channel_->setReadCallback(
      boost::bind(&TcpConnection::handleRead, this, _1));
  // 连接关闭，回调TcpConnection::handleClose
  channel_->setCloseCallback(
      boost::bind(&TcpConnection::handleClose, this));
  // 发生错误，回调TcpConnection::handleError
  channel_->setErrorCallback(
      boost::bind(&TcpConnection::handleError, this));
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd();
}

// 线程安全，可以跨线程调用
void TcpConnection::send(const void* data, size_t len)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(data, len);
    }
    else
    {
      // 如果不在当前IO线程，直接转到IO线程所属的EventLoop来调用
      string message(static_cast<const char*>(data), len);//跨线程调用会一定的开销
                                                          //需要把缓冲区message构造出来，传递过去
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,
                      message));
    }
  }
}

// 线程安全，可以跨线程调用
void TcpConnection::send(const StringPiece& message)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(message);
    }
    else
    {
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,
                      message.as_string()));
                    //std::forward<string>(message)));//若C++11标准的话，可改
    }
  }
}

// 线程安全，可以跨线程调用
void TcpConnection::send(Buffer* buf)
{
  if (state_ == kConnected)
  {
    // 在IO线程
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());//buf->peek()代表读的指针位置
      buf->retrieveAll();//移除缓冲区的内容
    }
    else
    {
      // 不在IO线程 
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,
                      buf->retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
  sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  // channel_->fd()是已连接socket
  sockets::write(channel_->fd(), data, len);
  
  /*
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool error = false;
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  // outputBuffer_缓冲区
  buffer类本身不是线程安全的，在处理数据的时候都没有加锁和解锁。
  原因是：buffer共给TcpConnection使用的，TcpConnection的发送方法send()会转到它所属的IO线程中操作，
  一个TcpConnection只能属于一个IO线程来处理，所以不用对buffer加锁，因为同一个时刻只有一个线程来操作，
  buffer是每个连接都私有的，而不是所有连接所共享的，所以它是不需要锁操作的。所以没必要把buffer设置成加锁的。

  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
  {
    nwrote = sockets::write(channel_->fd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_)
      {
        loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
      }
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK)
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE) // FIXME: any others?
        {
          error = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!error && remaining > 0)
  {
    LOG_TRACE << "I am going to write more data";
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
      loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);
    if (!channel_->isWriting())
    {
      channel_->enableWriting();
    }
  }
  */
}

// 不可以跨线程调用
/*
若oupt Buffer中有数据还没有发完，服务器端的连接状态更改为kDisconnecting，是并没有关闭连接的；
若服务器端主动断开与客户端的连接（close()或者shutdown()），这意味着客户端read()返回为0，接着客户端就close(conn)；
接着服务器端会收到2个事件revents：POLLHUP和POLLIN；

正常情况下，客户端关闭连接，服务器端只有收到POLLIN事件
可以在37\jmuduo\muduo\net\Channel.cc的handleEventWithGuard中进行验证

*/
void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)//当前处于连接的状态，这里多线程可能是访问的，可以写成原子性操作
  {
    setState(kDisconnecting);//设置为正在断开连接的状态
    // FIXME: shared_from_this()?
    loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));//在这个IO线程中调用shutdownInLoop
  }                                                                     //this指针可以更改为shared_from_this()
}

void TcpConnection::shutdownInLoop()
{
  // 断言在IO线程
  loop_->assertInLoopThread();

  // 不再关注POLLOUT事件
  if (!channel_->isWriting())
  {
    // we are not writing
    // 关闭写的一半
    socket_->shutdownWrite();
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  LOG_TRACE << "[3] usecount=" << shared_from_this().use_count();
  channel_->tie(shared_from_this());
  channel_->enableReading();	// TcpConnection所对应的通道加入到Poller关注

  connectionCallback_(shared_from_this());
  LOG_TRACE << "[4] usecount=" << shared_from_this().use_count();
}

void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();

    connectionCallback_(shared_from_this());
  }
  channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
    // receiveTime消息到来的时间，因为poll的时候会返回一个时间
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  else if (n == 0)
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }

  /*
  loop_->assertInLoopThread();
  int savedErrno = 0;
  char buf[65536];
  ssize_t n = ::read(channel_->fd(), buf, sizeof buf);
  if (n > 0)
  {
    messageCallback_(shared_from_this(), buf, n);
  }
  else if (n == 0)
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
  */
}

void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << state_;
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);		// 这一行，可以不调用
  LOG_TRACE << "[7] usecount=" << guardThis.use_count();
  // must be the last line
  closeCallback_(guardThis);	// 调用TcpServer::removeConnection
  LOG_TRACE << "[11] usecount=" << guardThis.use_count();
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}
