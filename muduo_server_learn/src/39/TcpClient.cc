// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/TcpClient.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Connector.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

// TcpClient::TcpClient(EventLoop* loop)
//   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }

namespace muduo
{
namespace net
{
namespace detail
{

void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
  loop->queueInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));
}

void removeConnector(const ConnectorPtr& connector)
{
  //connector->
}

}
}
}

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     const string& name)
  : loop_(CHECK_NOTNULL(loop)),
    connector_(new Connector(loop, serverAddr)),
    name_(name),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    retry_(false),
    connect_(true),
    nextConnId_(1)
{
  // 设置连接成功回调函数
  // 一旦连接建立成功，回调newConnection
  connector_->setNewConnectionCallback(
      boost::bind(&TcpClient::newConnection, this, _1));
  // FIXME setConnectFailedCallback
  LOG_INFO << "TcpClient::TcpClient[" << name_
           << "] - connector " << get_pointer(connector_);
}

TcpClient::~TcpClient()
{
  LOG_INFO << "TcpClient::~TcpClient[" << name_
           << "] - connector " << get_pointer(connector_);
  TcpConnectionPtr conn;
  {
    MutexLockGuard lock(mutex_);
    conn = connection_;
  }
  // 如果已经建立连接，销毁TcpConnection对象
  if (conn)
  {
    // FIXME: not 100% safe, if we are in different thread

	// 重新设置TcpConnection中的closeCallback_为detail::removeConnection
  // 因为TcpClient对象销毁了，TcpConnection对象也要跟着销毁
    CloseCallback cb = boost::bind(&detail::removeConnection, loop_, _1);
    loop_->runInLoop(
        boost::bind(&TcpConnection::setCloseCallback, conn, cb));
  }
  else
  {
    // 这种情况，说明connector处于未连接状态，将connector_停止
    connector_->stop();
    // FIXME: HACK
    loop_->runAfter(1, boost::bind(&detail::removeConnector, connector_));
  }
}

void TcpClient::connect()
{
  // FIXME: check state
  LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
           << connector_->serverAddress().toIpPort();
  connect_ = true;
  connector_->start();	// 发起连接
}

// 用于连接已建立的情况下，关闭连接
void TcpClient::disconnect()
{
  connect_ = false;

  {
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      connection_->shutdown();
    }
  }
}

// 连接尚未成功，停止发起连接
// 停止connector_
void TcpClient::stop()
{
  connect_ = false;
  connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary

  /*
  连接建立成功后，在Connector中，int sockfd = removeAndResetChannel();	// 从poller中移除关注，并将channel置空
  poller不再关注Connector中的channel对应的可写事件了，否则会出现busy-loop，因为对于水平触发而言，已连接socket会一直处于可写状态
  */
  // 连接建立成功，就会得到TcpConnection对象
  TcpConnectionPtr conn(new TcpConnection(loop_,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

  // 连接建立回调函数
  conn->setConnectionCallback(connectionCallback_);
  // 消息到达回调函数
  conn->setMessageCallback(messageCallback_);
  // 数据发送完毕回调函数
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  // 连接关闭回调函数
  conn->setCloseCallback(
      boost::bind(&TcpClient::removeConnection, this, _1)); // FIXME: unsafe
  {
    MutexLockGuard lock(mutex_);
    connection_ = conn;		// 保存TcpConnection
  }

  /*conn->connectEstablished();	作用：
  channel_->enableReading();	
                              // 关注channel_的可读事件
  connectionCallback_(shared_from_this());//回调connectionCallback_

  */
  conn->connectEstablished();		// 这里回调connectionCallback_
}

// 因为他是连接关闭后回调的一个函数
void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  assert(loop_ == conn->getLoop());

  {
    MutexLockGuard lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();//重置connection_
  }

  // 在IO线程中调用connectDestroyed
  loop_->queueInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));
  if (retry_ && connect_)
  {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to "
             << connector_->serverAddress().toIpPort();
    // 这里的重连是指连接建立成功之后被断开的重连
    connector_->restart();
  }
}

