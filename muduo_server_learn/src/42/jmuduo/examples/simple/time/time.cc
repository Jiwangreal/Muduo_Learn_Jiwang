#include "time.h"

#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

TimeServer::TimeServer(muduo::net::EventLoop* loop,
                             const muduo::net::InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "TimeServer")
{
  server_.setConnectionCallback(
      boost::bind(&TimeServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&TimeServer::onMessage, this, _1, _2, _3));
}

void TimeServer::start()
{
  server_.start();
}

void TimeServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
  LOG_INFO << "TimeServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    time_t now = ::time(NULL);//连接到达，就获取当前事件的s数
    int32_t be32 = sockets::hostToNetwork32(static_cast<int32_t>(now));//转换成网络字节序
    conn->send(&be32, sizeof be32);//发哦那个给客户端
    conn->shutdown();//仅关闭写入这一半
  }
}

void TimeServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " discards " << msg.size()
           << " bytes received at " << time.toString();
}

