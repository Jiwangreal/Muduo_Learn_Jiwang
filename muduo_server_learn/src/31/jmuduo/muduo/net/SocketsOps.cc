// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/SocketsOps.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Endian.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <strings.h>  // bzero
#include <sys/socket.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{

typedef struct sockaddr SA;

// 将网际地址指针，转换未通用地址指针
// 带const
const SA* sockaddr_cast(const struct sockaddr_in* addr)
{
  // 先隐式转成void*，再转成const SA*
  return static_cast<const SA*>(implicit_cast<const void*>(addr));
}

// 不带const
SA* sockaddr_cast(struct sockaddr_in* addr)
{
  return static_cast<SA*>(implicit_cast<void*>(addr));
}

// 将fd设置为非阻塞模式和FD_CLOEXEC
void setNonBlockAndCloseOnExec(int sockfd)
{
  // non-block
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);
  // FIXME check

  // close-on-exec
  flags = ::fcntl(sockfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFD, flags);
  // FIXME check

  (void)ret;
}

}

// 创建非阻塞的socket
int sockets::createNonblockingOrDie()
{
  // socket
  // VALGRIND宏表示valgrind工具，用于内存泄漏和文件描述符打开状态
#if VALGRIND
  int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
  //若用工具的话，最好使用下面的方式设置下socket状态
  setNonBlockAndCloseOnExec(sockfd);
#else
// 生产环境就不需要检测了，就不需要valgrind工具
  // Linux 2.6.27以上的内核支持SOCK_NONBLOCK与SOCK_CLOEXEC
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
#endif
  return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr_in& addr)
{
  int ret = ::bind(sockfd, sockaddr_cast(&addr), sizeof addr);
  if (ret < 0)
  {
    LOG_SYSFATAL << "sockets::bindOrDie";
  }
}

void sockets::listenOrDie(int sockfd)
{
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0)
  {
    LOG_SYSFATAL << "sockets::listenOrDie";
  }
}

int sockets::accept(int sockfd, struct sockaddr_in* addr)
{
  socklen_t addrlen = sizeof *addr;
#if VALGRIND
  int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
  setNonBlockAndCloseOnExec(connfd);
#else
  int connfd = ::accept4(sockfd, sockaddr_cast(addr),
                         &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
  if (connfd < 0)
  {
    int savedErrno = errno;
    LOG_SYSERR << "Socket::accept";//这里可能会去改变error的值，所以先把errno先保存下来
    switch (savedErrno)
    {
      case EAGAIN:
      case ECONNABORTED:
      case EINTR:
      case EPROTO: // ???
      case EPERM:
      case EMFILE: // per-process lmit of open file desctiptor ???
        // expected errors
        // 非致命错误
        errno = savedErrno;
        break;
      case EBADF:
      case EFAULT:
      case EINVAL:
      case ENFILE:
      case ENOBUFS:
      case ENOMEM:
      case ENOTSOCK:
      case EOPNOTSUPP:
        // unexpected errors
        // 下面式是致命错误
        LOG_FATAL << "unexpected error of ::accept " << savedErrno;
        break;
      default:
        LOG_FATAL << "unknown error of ::accept " << savedErrno;
        break;
    }
  }
  return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr_in& addr)
{
  return ::connect(sockfd, sockaddr_cast(&addr), sizeof addr);
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
  return ::read(sockfd, buf, count);
}

// readv与read不同之处在于，接收的数据可以填充到多个缓冲区中
// iov是一个数组，iovcnt是数组的个数，eg：第一个缓冲区不够用了， 就可以将其接收到第二个缓冲区中
ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
  return ::readv(sockfd, iov, iovcnt);
}

// 对应还有writev，这里没有封装
ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
  return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
  if (::close(sockfd) < 0)
  {
    LOG_SYSERR << "sockets::close";
  }
}

// 只关闭写的这一半
void sockets::shutdownWrite(int sockfd)
{
  if (::shutdown(sockfd, SHUT_WR) < 0)
  {
    LOG_SYSERR << "sockets::shutdownWrite";
  }
}

// 将地址addr转换成IP与端口的形式，保存到buf中
void sockets::toIpPort(char* buf, size_t size,
                       const struct sockaddr_in& addr)
{
  // INET_ADDRSTRLEN表示网际地址的长度
  char host[INET_ADDRSTRLEN] = "INVALID";
  
  // 将网际地址的addrip放入到host中
  toIp(host, sizeof host, addr);

  //addr.sin_port是网络字节序的端口
  uint16_t port = sockets::networkToHost16(addr.sin_port);//转换成主机字节序的端口
  snprintf(buf, size, "%s:%u", host, port);//格式化到buf中
}

void sockets::toIp(char* buf, size_t size,
                   const struct sockaddr_in& addr)
{
  assert(size >= INET_ADDRSTRLEN);
  /*与inet_ntoa区别是：
  char *inet_ntoa(struct in_addr in);
  将网络字节序的32bit的地址转换为char*（点分十进制）
  */
//  inet_ntop指定了IPv4协议族：AF_INET
  ::inet_ntop(AF_INET, &addr.sin_addr, buf, static_cast<socklen_t>(size));
}

//从IP和端口，构造一个网际协议地址addr
void sockets::fromIpPort(const char* ip, uint16_t port,
                           struct sockaddr_in* addr)
{
  addr->sin_family = AF_INET;
  addr->sin_port = hostToNetwork16(port);

  // 与之对应的是inet_aton，将点分十进制的IP地址转换为网络字节序的ip地址
  // inet_pton也可以支持ipv6，但是muduo没实现
  if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
  {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

int sockets::getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = sizeof optval;

  // 通过getsockopt来返回socket错误
  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    return errno;
  }
  else
  {
    // 调用成功，则错误返回到optval中
    return optval;
  }
}

// 对于一个已连接socket来讲，都有一个本地的地址和对等方的地址
struct sockaddr_in sockets::getLocalAddr(int sockfd)
{
  struct sockaddr_in localaddr;
  bzero(&localaddr, sizeof localaddr);
  socklen_t addrlen = sizeof(localaddr);
  // 获取本地地址
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getLocalAddr";
  }
  return localaddr;
}

// 对于一个已连接socket来讲，都有一个本地的地址和对等方的地址
struct sockaddr_in sockets::getPeerAddr(int sockfd)
{
  struct sockaddr_in peeraddr;
  bzero(&peeraddr, sizeof peeraddr);
  socklen_t addrlen = sizeof(peeraddr);
  // 获取对等方地址
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getPeerAddr";
  }
  return peeraddr;
}

// 自连接是指(sourceIP, sourcePort) = (destIP, destPort)
// 自连接发生的原因:
// 客户端在发起connect的时候，没有bind(2)
// 客户端与服务器端在同一台机器，即sourceIP = destIP，
// 服务器尚未开启，即服务器还没有在destPort端口上处于监听
// 就有可能出现自连接，这样，服务器也无法启动了

bool sockets::isSelfConnect(int sockfd)
{
  struct sockaddr_in localaddr = getLocalAddr(sockfd);
  struct sockaddr_in peeraddr = getPeerAddr(sockfd);
  return localaddr.sin_port == peeraddr.sin_port
      && localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
}

