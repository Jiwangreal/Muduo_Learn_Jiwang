// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_HTTP_HTTPSERVER_H
#define MUDUO_NET_HTTP_HTTPSERVER_H

#include <muduo/net/TcpServer.h>
#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

class HttpRequest;
class HttpResponse;

/// A simple embeddable HTTP server designed for report status of a program.
/// It is not a fully HTTP 1.1 compliant server, but provides minimum features
/// that can communicate with HttpClient and Web browser.
/// It is synchronous, just like Java Servlet.

class HttpServer : boost::noncopyable
{
 public:
  typedef boost::function<void (const HttpRequest&,
                                HttpResponse*)> HttpCallback;

  HttpServer(EventLoop* loop,
             const InetAddress& listenAddr,
             const string& name);

  ~HttpServer();  // force out-line dtor, for scoped_ptr members.

  /// Not thread safe, callback be registered before calling start().
  void setHttpCallback(const HttpCallback& cb)
  {
    httpCallback_ = cb;
  }

  // 还支持多线程
  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start();

 private:
  void onConnection(const TcpConnectionPtr& conn);
  // 当服务端收到客户端发来的http客户端请求，首先回调onMessage，这是一个TCP服务器
  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp receiveTime);
  //  在onMessage()回调onRequest()，在onRequest()回调用户的httpCallback_
  void onRequest(const TcpConnectionPtr&, const HttpRequest&);

  //HttpServer服务器也是一个tcp服务器，在应用层使用http协议，在传输层使用tcp协议
  // 所以包含TcpServer server_;
  TcpServer server_;
  // 使用基于对象的编程思想，并不是去继承HttpServer来实现自己的http服务器，而是包含一个HttpServer对象
  HttpCallback httpCallback_;	// 在处理http请求（即调用onRequest）的过程中回调此函数，对请求进行具体的处理
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPSERVER_H
