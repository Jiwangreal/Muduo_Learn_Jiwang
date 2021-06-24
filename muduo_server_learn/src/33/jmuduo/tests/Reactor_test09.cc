#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 这里采用基于对象的编程思想，如果是面向对象编程思想：这里应该继承TcpServer，然后去覆盖onConnection和onMessage方法
class TestServer
{
 public:
  TestServer(EventLoop* loop,
             const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "TestServer")
  {
    server_.setConnectionCallback(
        boost::bind(&TestServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&TestServer::onMessage, this, _1, _2, _3));
  }

  void start()
  {
	  server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      printf("onConnection(): new connection [%s] from %s\n",
             conn->name().c_str(),
             conn->peerAddress().toIpPort().c_str());
    }
    else
    {
      printf("onConnection(): connection [%s] is down\n",
             conn->name().c_str());
    }
  }

  void onMessage(const TcpConnectionPtr& conn,
                   const char* data,
                   ssize_t len)
  {
    printf("onMessage(): received %zd bytes from connection [%s]\n",
           len, conn->name().c_str());
  }

  EventLoop* loop_;
  TcpServer server_;
};


int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(8888);
  EventLoop loop;

  // 相较于33\jmuduo\tests\Reactor_test08.cc封装了下
  TestServer server(&loop, listenAddr);
  server.start();

  loop.loop();
}