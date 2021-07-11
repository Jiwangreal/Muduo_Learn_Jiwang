#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

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
    server_.setThreadInitCallback(boost::bind(&ChatServer::threadInit, this, _1));
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    // 因为每一个线程都有一个线程实例connections_.instance()，所以不用锁来保护
    if (conn->connected())
    {
      connections_.instance().insert(conn);
    }
    else
    {
      connections_.instance().erase(conn);
    }
  }
/*
以前的所有方案：
C1——> S hello T1 线程 来装啊消息给所有的客户端

现在的方案：假设服务端的有4个线程
C1——> S hello T1 线程转发消息给C1，T12线程转发消息给C2，
              T3线程转发消息给C3，T4 线程转发消息给C4
              T1线程转发消息给C5，T2 线程转发消息给C6，

*/
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    EventLoop::Functor f = boost::bind(&ChatServer::distributeMessage, this, message);
    LOG_DEBUG;

    // 这意味着这个锁所锁定的区域并没有去转发消息，就可以打打的去提高并发，并不需要转发完毕，另外一个线程才进入到onStringMessage()
    // 大大提高并发
    MutexLockGuard lock(mutex_);
    // 转发消息给所有客户端，高效转发（多线程来转发），下面的代码执行速度很快，锁竞争很小
    for (std::set<EventLoop*>::iterator it = loops_.begin();
        it != loops_.end();
        ++it)
    {
      // 1、让对应的IO线程来执行distributeMessage
	  // 2、distributeMessage放到IO线程队列中执行，因此，这里的mutex_锁竞争大大减小
	  // 3、distributeMessage不受mutex_保护
      (*it)->queueInLoop(f);//(*it)就是从EventLoop列表中取出的EventLoop对象
                            // 加入到队列执行，立刻执行会增大锁占用的时间，降低并发度
    }
    LOG_DEBUG;
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;

  void distributeMessage(const string& message)
  {
    LOG_DEBUG << "begin";
    // connections_是thread local变量，所以不需要保护
    for (ConnectionList::iterator it = connections_.instance().begin();
        it != connections_.instance().end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
    LOG_DEBUG << "end";
  }

  // IO线程运行之前先回调threadInit()
  void threadInit(EventLoop* loop)
  {
    assert(connections_.pointer() == NULL);
    connections_.instance();
    assert(connections_.pointer() != NULL);
    MutexLockGuard lock(mutex_);
    loops_.insert(loop);//将IO线程所对应的EventLoop对象插入到loops_列表
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  // 线程局部单例变量，每个线程都有一个connections_实例
  ThreadLocalSingleton<ConnectionList> connections_;

  MutexLock mutex_;
  std::set<EventLoop*> loops_;	// EventLoop列表
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


