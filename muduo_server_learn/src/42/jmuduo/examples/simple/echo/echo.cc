#include "echo.h"

#include <muduo/base/Logging.h>

#include <boost/bind.hpp>

// using namespace muduo;
// using namespace muduo::net;

EchoServer::EchoServer(muduo::net::EventLoop* loop,
                       const muduo::net::InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer")
{
  // 连接建立或者连接断开的回调函数，绑定到EchoServer::onConnection
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));

  //消息到达的回调函数，绑定到 EchoServer::onMessage
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::start()
{
  server_.start();
}

// 相关的应用程序的逻辑在onConnection()和onMessage()中实现
void EchoServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");//连接建立打印输出up，连接断开打印输出down
}

void EchoServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
  muduo::string msg(buf->retrieveAllAsString());//buf->retrieveAllAsString()表示从缓冲区中取回
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
           << "data received at " << time.toString();
  conn->send(msg);
}

