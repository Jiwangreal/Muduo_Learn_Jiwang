// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/Mutex.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() { return localAddr_; }
  const InetAddress& peerAddress() { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }

  // void send(string&& message); // C++11
  void send(const void* message, size_t len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  void setTcpNoDelay(bool on);

  void setContext(const boost::any& context)
  { context_ = context; }

  // const成员函数是不会更改context_对象的
  // 返回const，说明外部不会去更改context_对象
  const boost::any& getContext() const
  { return context_; }

  // mutable含义是get之后，可以更改context_
  boost::any* getMutableContext()
  { return &context_; }

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  Buffer* inputBuffer()
  { return &inputBuffer_; }

  /// Internal use only.
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  void setState(StateE s) { state_ = s; }

  EventLoop* loop_;			// 所属EventLoop
  string name_;				// 连接名
  StateE state_;  // FIXME: use atomic variable
  // we don't expose those classes to client.
  boost::scoped_ptr<Socket> socket_;
  boost::scoped_ptr<Channel> channel_;
  InetAddress localAddr_;
  InetAddress peerAddr_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;


 /*
 上层应用发送数据，只管调用conn->send()来发送，只有网络库知道将数据拷贝到了内核缓冲区，拷贝完毕后，会通过writeCompleteCallback_
 通知上层应用程序，以便上层应用程序发送更多的数据；
  通常大流量的应用程序才需要关注writeCompleteCallback_；
  （1）大流量的应用程序
  不断生成数据，然后发送conn->send();
  如果对等方接收不及时，受到通告窗口的控制，内核发送缓冲区不足，这个时候，就会将用户数据添加到应用层发送缓冲区(output buffer)，可能会
  撑爆output buffer；

  解决办法是：调整发送频率：只需要关注writeCompleteCallback_
  当所有的数据都拷贝到内核缓冲区的时候，上层的应用程序通过writeCompleteCallback_得到通知，此时再发送数据，能够保证所有的数据都发送完再继续发送；

  （2）小流量的应用程序
 不需要关注writeCompleteCallback_
 */
  WriteCompleteCallback writeCompleteCallback_;		// 数据发送完毕回调函数，即所有的用户数据都已拷贝到内核缓冲区时回调该函数
													// outputBuffer_被清空也会回调该函数(意味着所有的数据都拷贝到内核缓冲区了)，可以理解为低水位标回调函数（即：outputBuffer_被清空，没有数据）
  HighWaterMarkCallback highWaterMarkCallback_;	    // 高水位标回调函数
                                                    //outputBuffer_撑到一定程度的时候最好调用下，这意味着对等方接收不及时导致outputBuffer_不断增大，
                                                    // 很可能没有关注writeCompleteCallback_的时候会出现该情况
                                                    // 不断增大，此时可以在highWaterMarkCallback_中可以断开与对等方的连接，你面内存不断的增大撑爆OS

  CloseCallback closeCallback_;
  size_t highWaterMark_;		// 高水位标
  Buffer inputBuffer_;			// 应用层接收缓冲区
  Buffer outputBuffer_;			// 应用层发送缓冲区
  boost::any context_;			// 表示连接对象可以绑定一个未知类型的上下文对象
                            // 提供一个接口，上层的应用程序可以绑定一个未知类型的上下文对象
};

typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
