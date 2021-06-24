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
/*
void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
  buf->retrieveAll();
}
*/
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
      boost::bind(&TcpConnection::handleClose, this));//连接关闭，回调handleClose
  // 发生错误，回调TcpConnection::handleError
  channel_->setErrorCallback(
      boost::bind(&TcpConnection::handleError, this));//发生错误，回调handleError
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd();
}

// 当连接到来的时候，连接是connectEstablished()
void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  LOG_TRACE << "[3] usecount=" << shared_from_this().use_count();//获得一个由当前对象转化而来的shared_ptr对象，引用计数+1=3
                                                                 //但该对象是临时对象，马上销毁了，又变成了2

  //这里引用计数=3
  channel_->tie(shared_from_this());//shared_from_this()方法来自：继承boost::enable_shared_from_this<TcpConnection>
                                    //将当前的TcpConnection对象转化为shared_ptr，不能直接用this
                                    // 获得自身对象的shared_ptr对象，即tie(shared_ptr<TcpConnection>)

  // shared_from_this()是一个临时对象，引用计数=2
  channel_->enableReading();	// TcpConnection所对应的通道加入到Poller关注

  // shared_from_this()也是一个临时对象，使用时引用计数=3，销毁时引用计数=2
  connectionCallback_(shared_from_this());//connectionCallback_用户回调函数
  LOG_TRACE << "[4] usecount=" << shared_from_this().use_count();//引用计数=3
}//跳出connectEstablished()，引用计数=2


void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();

    connectionCallback_(shared_from_this());//回调用户的回调函数
                                            //这里应该不会调用的，因为handleClose()中setState(kDisconnected);已经将状态设置为kDisconnected
                                            //此外handleClose()中connectionCallback_(guardThis);	也回调了用户的回调函数
                                            // 弱没有上面2行代码，则会进入到connectDestroyed()里面去回调用户的回调函数
  }
  channel_->remove();//从poll中移除
}

// 弱没有处理连接断开，会处于busy-loop的状态
void TcpConnection::handleRead(Timestamp receiveTime)
{
  /*
  loop_->assertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
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
  */
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
    handleClose();//处理连接断开
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();//处理错误
  }
  
}

void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << state_;
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);// connectionCallback_(guardThis);		// 这一行，可以不调用,那么这里也不应该调用
  channel_->disableAll();


  //这么操作的原因是把之前shared_ptr对象的引用计数值+1，若写成：TcpConnectionPtr guardThis(this);则不对，因为构造了一个新对象，这里引用计数=1，而不是+1
  // 见
  TcpConnectionPtr guardThis(shared_from_this());//shared_ptr<TcpConnection>,返回自身对象的shared_ptr

  // 这里也可以回调用户的函数，于时序图中10：connCb()的位置一样回调用户的函数
  // 回调用户连接到来的一个回调函数
  connectionCallback_(guardThis);		// 这一行，可以不调用，eg：34\jmuduo\tests\Reactor_test09.cc中回调onConnection()
  LOG_TRACE << "[7] usecount=" << guardThis.use_count();
  // must be the last line
  // 内部的回调函数
  closeCallback_(guardThis);	// 调用TcpServer::removeConnection，因为TcpServer连接到来时，设置了conn->setCloseCallback(boost::bind(&TcpServer::removeConnection, this, _1));
  LOG_TRACE << "[11] usecount=" << guardThis.use_count();
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}
