// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include <muduo/base/Types.h>
#include <muduo/net/TcpConnection.h>

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
class TcpServer : boost::noncopyable
{
 public:
  //typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  //TcpServer(EventLoop* loop, const InetAddress& listenAddr);
  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,//地址
            const string& nameArg);//名称
  ~TcpServer();  // force out-line dtor, for scoped_ptr members.

  const string& hostport() const { return hostport_; }
  const string& name() const { return name_; }

  /// Starts the server if it's not listenning.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  /// Set connection callback.
  /// Not thread safe.
  // 设置连接到来或者连接关闭回调函数
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  // 设置消息到来回调函数
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }


 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd, const InetAddress& peerAddr);//连接到来的时候回调的函数

  // <连接的名称，连接对象的指针>
  typedef std::map<string, TcpConnectionPtr> ConnectionMap;

  EventLoop* loop_;  // the acceptor loop，acceptor_所属的EventLoop
  const string hostport_;		// 服务端口
  const string name_;			// 服务名
  boost::scoped_ptr<Acceptor> acceptor_; // avoid revealing Acceptor，用智能指针来管理acceptor_
  ConnectionCallback connectionCallback_;//连接到来的回调函数
  MessageCallback messageCallback_;//消息到来的回调函数

  bool started_;//是否已经启动
  // always in loop thread
  int nextConnId_;				// 下一个连接ID
  ConnectionMap connections_;	// 连接列表
};

}
}

#endif  // MUDUO_NET_TCPSERVER_H
