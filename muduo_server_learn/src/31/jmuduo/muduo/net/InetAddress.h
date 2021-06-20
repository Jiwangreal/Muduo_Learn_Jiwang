// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_INETADDRESS_H
#define MUDUO_NET_INETADDRESS_H

#include <muduo/base/copyable.h>
#include <muduo/base/StringPiece.h>

#include <netinet/in.h>

namespace muduo
{
namespace net
{

///
/// Wrapper of sockaddr_in.
///
/// This is an POD interface class.
// 网际地址的封装
class InetAddress : public muduo::copyable
{
 public:
  /// Constructs an endpoint with given port number.
  /// Mostly used in TcpServer listening.
  // 仅仅指定port，不指定ip，则ip为INADDR_ANY（即0.0.0.0）
  explicit InetAddress(uint16_t port);

  /// Constructs an endpoint with given ip and port.
  /// @c ip should be "1.2.3.4"
  // StringPiece既可以接收char*参数，也可以接收string参数
  InetAddress(const StringPiece& ip, uint16_t port);

  /// Constructs an endpoint with given struct @c sockaddr_in
  /// Mostly used when accepting new connections
  InetAddress(const struct sockaddr_in& addr)
    : addr_(addr)
  { }

  // 转换成ip
  string toIp() const;
  // 转换成IP+端口
  string toIpPort() const;

  // __attribute__ ((deprecated)) 表示该函数是过时的，被淘汰的
  // 这样使用该函数，在编译的时候，会发出警告
  // 上层的程序调用了toHostPort()，编译的时候会发生警告
  // 主要用于软件升级的时候，告诉程序员尽量别使用这个函数，应该使用toIpPort()函数
  string toHostPort() const __attribute__ ((deprecated))
  { return toIpPort(); }

  // default copy/assignment are Okay

  const struct sockaddr_in& getSockAddrInet() const { return addr_; }
  void setSockAddrInet(const struct sockaddr_in& addr) { addr_ = addr; }

  // 返回网络字节序的ip
  uint32_t ipNetEndian() const { return addr_.sin_addr.s_addr; }
  
  // 返回网络字节序的端口
  uint16_t portNetEndian() const { return addr_.sin_port; }

 private:
  struct sockaddr_in addr_;//InetAddress是对struct sockaddr_in的封装
};

}
}

#endif  // MUDUO_NET_INETADDRESS_H
