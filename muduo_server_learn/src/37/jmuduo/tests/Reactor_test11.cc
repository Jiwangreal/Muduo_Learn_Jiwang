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
             const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "TestServer")
  {
    // 若这里没有设置回调函数，会调用默认的回调函数connectionCallback_(defaultConnectionCallback)messageCallback_(defaultMessageCallback),
    // 而defaultConnectionCallback的定义在TcpConnection的void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)，声明在
    //Callbacks的 void defaultConnectionCallback(const TcpConnectionPtr& conn);
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
                 Buffer* buf,
                 Timestamp receiveTime)
  {
    string msg(buf->retrieveAllAsString());//获取缓冲区的数据打印一下
    printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
           msg.size(),
           conn->name().c_str(),//连接的名称
           receiveTime.toFormattedString().c_str());//接收的时间
    conn->send(msg);//回射回去
  }

  EventLoop* loop_;
  TcpServer server_;
};


int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(8888);
  EventLoop loop;

  TestServer server(&loop, listenAddr);
  server.start();

  loop.loop();
}