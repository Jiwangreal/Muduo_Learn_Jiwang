// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Socket.h>

#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>  // bzero

using namespace muduo;
using namespace muduo::net;

Socket::~Socket()
{
  sockets::close(sockfd_);
}

void Socket::bindAddress(const InetAddress& addr)
{
  sockets::bindOrDie(sockfd_, addr.getSockAddrInet());
}

void Socket::listen()
{
  sockets::listenOrDie(sockfd_);
}

// InetAddress对网际地址的封装，上层应用看不到 struct sockaddr_in，而是看到的是InetAddress
int Socket::accept(InetAddress* peeraddr)
{
  struct sockaddr_in addr;
  bzero(&addr, sizeof addr);
  int connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0)
  {
    peeraddr->setSockAddrInet(addr);
  }
  return connfd;
}

void Socket::shutdownWrite()
{
  // sockets是名称空间
  sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on)
{
  //on若是true，则是禁用Nagle算法
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
               &optval, sizeof optval);
  // FIXME CHECK
}

void Socket::setReuseAddr(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof optval);
  // FIXME CHECK
}

void Socket::setKeepAlive(bool on)
{
  // on=true表示开启tcp层的心跳
  // 应用层有心跳，可以不设置
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, sizeof optval);
  // FIXME CHECK
}

