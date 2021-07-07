// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_HTTP_HTTPCONTEXT_H
#define MUDUO_NET_HTTP_HTTPCONTEXT_H

#include <muduo/base/copyable.h>

#include <muduo/net/http/HttpRequest.h>

namespace muduo
{
namespace net
{

// 协议解析类
class HttpContext : public muduo::copyable
{
 public:
  enum HttpRequestParseState
  {
    kExpectRequestLine,//当前处于解析请求行的状态
    kExpectHeaders,//正处于解析头部信息的状态
    kExpectBody,//正处于解析实体的状态
    kGotAll,//全部都解析完毕
  };

  HttpContext()
    : state_(kExpectRequestLine)
  {
  }

  // default copy-ctor, dtor and assignment are fine

  // 将状态设置未kExpectRequestLine
  bool expectRequestLine() const
  { return state_ == kExpectRequestLine; }

  bool expectHeaders() const
  { return state_ == kExpectHeaders; }

  bool expectBody() const
  { return state_ == kExpectBody; }

  bool gotAll() const
  { return state_ == kGotAll; }

  // 下个期望接收的状态是kExpectHeaders
  void receiveRequestLine()
  { state_ = kExpectHeaders; }

  // 一旦header接收了，下一个期望接收的状态就是kGotAll
  // 当前没有处理带有实体body的请求
  void receiveHeaders()
  { state_ = kGotAll; }  // FIXME

  // 重置HttpContext状态
  void reset()
  {
    state_ = kExpectRequestLine;
    HttpRequest dummy;//dummy虚的，假的意思
    request_.swap(dummy);//将当前对象置空
  }

  const HttpRequest& request() const
  { return request_; }

  HttpRequest& request()
  { return request_; }

 private:
  HttpRequestParseState state_;		// 请求解析状态
  HttpRequest request_;				// http请求
                            // 对该http请求进行解析
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPCONTEXT_H
