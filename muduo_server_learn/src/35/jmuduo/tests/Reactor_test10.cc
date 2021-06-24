#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class TestServer
{
 public:
  TestServer(EventLoop* loop,
             const InetAddress& listenAddr, int numThreads)//numThreads:线程池中创建的IO线程的个数
    : loop_(loop),
      server_(loop, listenAddr, "TestServer"),
      numThreads_(numThreads)
  {
    server_.setConnectionCallback(
        boost::bind(&TestServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&TestServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);//这里可以直接设置是单线程还是多线程模式
  }

  void start()
  {
	  server_.start();//启动服务器
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
  int numThreads_;
};


int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(8888);
  EventLoop loop;//主线程构造一个EventLoop对象

  TestServer server(&loop, listenAddr,4);//主线程构造一个TestServer对象,创建了4个IO线程,总的IO线程个数=5
  server.start();

  loop.loop();//5个事件循环:1个主线程+4个内部的IO线程,所以总共有5个EventLoop对象
}