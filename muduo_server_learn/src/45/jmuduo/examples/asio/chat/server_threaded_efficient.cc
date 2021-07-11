#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
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
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3)),
    connections_(new ConnectionList)//shared_ptr的引用计数=1，对connections_列表有2个操作：一个读操作，一个写操作
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
//  写操作，是一个线程
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);
    // 保护下面所有的临界区
    // connections_构造的时候，引用计数=1了，所以至少为1
    if (!connections_.unique())		// 说明引用计数大于1，若此时=2
    {
      // new ConnectionList(*connections_)这段代码拷贝了一份ConnectionList
      connections_.reset(new ConnectionList(*connections_));//*connections_就是ConnectionList对象，将原来的connections_的内容拷贝构造到ConnectionList
                                                            //ConnectionList用connections_来接管：connections_.reset()
                                                            // 新的connections_引用计数-1=1，原来的connections_也减为1了
    }
    // 断言引用计数=1，可以在副本上直接修改
    assert(connections_.unique());

    // 在复本上修改，不会影响读者，所以读者在遍历列表的时候，不需要用mutex保护
    if (conn->connected())
    {
      connections_->insert(conn);
    }
    else
    {
      connections_->erase(conn);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;
  typedef boost::shared_ptr<ConnectionList> ConnectionListPtr;//采用引用计数的智能指针

  // 读操作，是一个线程
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 引用计数加1=2，mutex保护的临界区大大缩短，提高了并发，相较于45\jmuduo\examples\asio\chat\server_threaded.cc
    ConnectionListPtr connections = getConnectionList();//栈上变量
    // 下面的代码不受到保护了
    // 可能大家会有疑问，不受mutex保护，写者更改了连接列表怎么办？
    // 实际上，写者是在另一个复本上修改，所以无需担心。
    // 缺点：此外，eg：hello消息到达第一个客户端与最后一个客户端之间的延迟仍然比较大
    // 因为向各个客户端转发仍然在同一个线程中执行的，可以让他在各个客户端所对应的线程中执行，下面可以封装到多线程中，多线程执行
    // 见：45\jmuduo\examples\asio\chat\server_threaded_highperformance.cc
    for (ConnectionList::iterator it = connections->begin();
        it != connections->end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }

	// 这个断言不一定成立
	//assert(!connections.unique());
    // 当connections这个栈上的变量销毁的时候，引用计数减1=0（有写操作的情况下去分析），则原来的connections_就会销毁掉，所以不会存在两份副本
  }

  ConnectionListPtr getConnectionList()
  {
    MutexLockGuard lock(mutex_);
    return connections_;//可能多个线程访问
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  ConnectionListPtr connections_;
};

// 核心是降低锁竞争
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

