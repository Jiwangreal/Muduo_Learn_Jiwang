#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
  {
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    // 有多个IO线程，因而这里的connections_需要用mutex保护
    MutexLockGuard lock(mutex_);
    if (conn->connected())
    {
      connections_.insert(conn);
    }
    else
    {
      connections_.erase(conn);
    }
  }

  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 有多个IO线程，因而这里的connections_需要用mutex保护，一直保护到send()发送
    MutexLockGuard lock(mutex_);
    // 转发消息给所有客户端
    for (ConnectionList::iterator it = connections_.begin();
        it != connections_.end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;
  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  ConnectionList connections_;		// 连接列表
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    if (argc > 2)
    {
      server.setThreadNum(atoi(argv[2]));
    }
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port [thread_num]\n", argv[0]);
  }
}

