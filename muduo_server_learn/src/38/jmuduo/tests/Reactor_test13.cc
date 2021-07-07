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
    server_.setConnectionCallback(
        boost::bind(&TestServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&TestServer::onMessage, this, _1, _2, _3));
	server_.setWriteCompleteCallback(
      boost::bind(&TestServer::onWriteComplete, this, _1));

    //这是一个chargen服务器程序，即是一个字符发送器协议
    // 生成数据
    string line;
    for (int i = 33; i < 127; ++i)
    {
      line.push_back(char(i));
    }
    line += line;

    for (size_t i = 0; i < 127-33; ++i)
    {
      message_ += line.substr(i, 72) + '\n';//生成数据保存到message_
    }
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

      conn->setTcpNoDelay(true);
      conn->send(message_);//一旦客户端连接过来，就发送这些数据
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
    muduo::string msg(buf->retrieveAllAsString());
	printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
           msg.size(),
           conn->name().c_str(),
           receiveTime.toFormattedString().c_str());

    conn->send(msg);
  }

  // 一旦onConnection::conn->send(message_);的所有数据都拷贝到内核发送缓冲区了，接着会调用onWriteComplete()
  void onWriteComplete(const TcpConnectionPtr& conn)
  {
    conn->send(message_);
  }

  EventLoop* loop_;
  TcpServer server_;

  muduo::string message_;
};

// 模拟大流量的程序
int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(8888);
  EventLoop loop;

  TestServer server(&loop, listenAddr);
  server.start();

  loop.loop();
}