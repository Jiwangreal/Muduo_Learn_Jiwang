// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie()),//创建一个监听socket
    acceptChannel_(loop, acceptSocket_.fd()),//关注acceptSocket_的事件
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);//设置地址重复调用
  acceptSocket_.bindAddress(listenAddr);//bind
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this));//acceptChannel_设置一个handleRead读的回调函数
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();//将所有的事件disable掉，才能调用remove()
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();//关注acceptChannel_的可读事件，当可读事件到来的时候会回调先前bind的handleRead()函数
}

void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(0);
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);//将对等方的地址返回到peerAddr
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);//回调应用层的回调函数
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of livev.
    // 处理失败，EMFILE表示太多的fd
    if (errno == EMFILE)
    {
      // 先关闭一个空闲的fd，目的是让accept()能够接收到
      // 由于设置的是电平触发，这个accept会一直触发，所以采用这个机制；
      // 一accept该fd，就关闭掉该fd，因为fd太多了，还是要准备一个空闲的fd
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

