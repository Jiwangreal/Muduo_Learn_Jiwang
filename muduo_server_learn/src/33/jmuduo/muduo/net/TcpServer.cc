// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
//#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg)
  : loop_(CHECK_NOTNULL(loop)),//检查loop不是空指针
    hostport_(listenAddr.toIpPort()),//端口号
    name_(nameArg),//名称
    acceptor_(new Acceptor(loop, listenAddr)),
    /*threadPool_(new EventLoopThreadPool(loop)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),*/
    started_(false),
    nextConnId_(1)
{
  // 对acceptor_设置一个回调函数,即Acceptor::handleRead函数注册的回调函数是newConnection()
  // 当一个连接事件到来的时候，poller.poll()返回一个活跃的通道，调用channel.handleEvent(),
  // 由于连接事件是一个可读事件，又会调用acceptor_.handleRead(),
  // Acceptor::handleRead函数中会回调用TcpServer::newConnection
  // _1对应的是socket文件描述符，_2对应的是对等方的地址(InetAddress)
  //acceptor_中的newConnectionCallback_(connfd, peerAddr);//这里会传递socket，以及对等方的地址
  acceptor_->setNewConnectionCallback(
      boost::bind(&TcpServer::newConnection, this, _1, _2));
    
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";
}

// 该函数多次调用是无害的，因为Acceptor.listen()中：listenning_ = true;第二个if进不来的
// 该函数可以跨线程调用
void TcpServer::start()
{
  if (!started_)
  {
    started_ = true;
  }

  if (!acceptor_->listenning())//若没有处于监听的状态，让它去执行Acceptor::listen
  {
	// get_pointer返回原生指针
  //通过原生指针调用 listen
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));//acceptor_是一个智能指针
  }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
  loop_->assertInLoopThread();
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", hostport_.c_str(), nextConnId_);//端口+下一个id
  ++nextConnId_;
  string connName = name_ + buf;//当前连接的名称

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  // 构造本地地址
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  // 构造一个连接对象
  TcpConnectionPtr conn(new TcpConnection(loop_,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  // 将连接对象conn放到列表connections_中
  connections_[connName] = conn;

  // 外层应用不会直接调用TcpConnection的setConnectionCallback()和setMessageCallback()
  // 外层应用只会去调用TcpServer的setConnectionCallback和setMessageCallback，再赋给TcpConnection
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);

  conn->connectEstablished();
}

