// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/http/HttpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/http/HttpContext.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpResponse.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

namespace muduo
{
namespace net
{
namespace detail
{

// 解析请求行
// FIXME: move to HttpContext class
bool processRequestLine(const char* begin, const char* end, HttpContext* context)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');//查找空格所在位置
  HttpRequest& request = context->request();//取出请求对象
  if (space != end && request.setMethod(start, space))		// 解析请求方法，即得到：GET
  {
    start = space+1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
      request.setPath(start, space);	// 解析PATH，即得到：/
      start = space+1;
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          request.setVersion(HttpRequest::kHttp11);		// HTTP/1.1
        }
        else if (*(end-1) == '0')
        {
          request.setVersion(HttpRequest::kHttp10);		// HTTP/1.0
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

// 更好的方式：FIXME: move to HttpContext class
// return false if any error
bool parseRequest(Buffer* buf, HttpContext* context, Timestamp receiveTime)
{
  bool ok = true;
  bool hasMore = true;
  while (hasMore)
  {
    if (context->expectRequestLine())	// 处于解析请求行状态
    {
      const char* crlf = buf->findCRLF();//查找\r\n
      if (crlf)
      {
        ok = processRequestLine(buf->peek(), crlf, context);	// 解析请求行
        if (ok)
        {
          context->request().setReceiveTime(receiveTime);		// 请求对象设置请求时间
          buf->retrieveUntil(crlf + 2);		// 将请求行从buf中取回，包括\r\n
          context->receiveRequestLine();	// 将HttpContext状态改为kExpectHeaders
        }                                 //while循环下来，就执行else if (context->expectHeaders())，相当于一个状态机
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
    else if (context->expectHeaders())		// 解析header
    {
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        const char* colon = std::find(buf->peek(), crlf, ':');		//冒号所在位置
        if (colon != crlf)
        {
          // peek()是头部位置，colon是:位置，crlf是\r所在位置
          context->request().addHeader(buf->peek(), colon, crlf);//请求对象context将Header添加进去，放到map中
        }
        else
        {
          // empty line, end of header
          context->receiveHeaders();		// HttpContext将状态改为kGotAll，这里并没有处理请求的实体
          hasMore = !context->gotAll();
        }
        buf->retrieveUntil(crlf + 2);		// 将header从buf中取回，包括\r\n
      }
      else
      {
        hasMore = false;
      }
    }
    else if (context->expectBody())			// 当前还不支持请求中带body
    {
      // FIXME:
    }
  }
  return ok;
}

void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
  resp->setStatusCode(HttpResponse::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

}
}
}


HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& name)
  : server_(loop, listenAddr, name),
    httpCallback_(detail::defaultHttpCallback)
{
  // 连接到来回调
  server_.setConnectionCallback(
      boost::bind(&HttpServer::onConnection, this, _1));
  // 消息到来回调
  server_.setMessageCallback(
      boost::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
  LOG_WARN << "HttpServer[" << server_.name()
    << "] starts listenning on " << server_.hostport();
  server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    conn->setContext(HttpContext());	// boost::any能够接收任意类型对象，TcpConnection与一个HttpContext绑定
                                      //HttpContext()用以构造一个http上下文对象, 用来解析http请求
  }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  // 取出该请求
  HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());

  // 让context上下文请求这个包
  if (!detail::parseRequest(buf, context, receiveTime))
  {
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
    conn->shutdown();
  }

  // 若请求成功，则context则包含了一个请求对象

  // 请求消息解析完毕
  if (context->gotAll())
  {
    // (连接，请求对象)
    onRequest(conn, context->request());
    context->reset();		// 本次请求处理完毕，重置HttpContext，适用于长连接
  }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
  // 取出头部
  const string& connection = req.getHeader("Connection");
  // http100版本不支持Keep-Alive，只支持短连接；1.1版本才支持Keep-Alive，长连接
  bool close = connection == "close" ||
    (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
  HttpResponse response(close);
  httpCallback_(req, &response);//回调用户函数对http请求进行对应处理，返回一个response对象（输入输出参数）
  Buffer buf;
  response.appendToBuffer(&buf);
  conn->send(&buf);//将buf发送给客户端
  if (response.closeConnection())//response是短连接，则shutdown
  {
    conn->shutdown();
  }
}

