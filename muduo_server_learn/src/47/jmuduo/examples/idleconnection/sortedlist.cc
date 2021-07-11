#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <boost/bind.hpp>
#include <list>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// RFC 862
class EchoServer
{
 public:
  EchoServer(EventLoop* loop,
             const InetAddress& listenAddr,
             int idleSeconds);

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn);

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp time);

  void onTimer();

  void dumpConnectionList() const;

  typedef boost::weak_ptr<TcpConnection> WeakTcpConnectionPtr;
  typedef std::list<WeakTcpConnectionPtr> WeakConnectionList;

  struct Node : public muduo::copyable
  {
    Timestamp lastReceiveTime; // 该连接最后一次活跃时刻，最后一次收到消息的时刻
    WeakConnectionList::iterator position; // 该连接在连接列表中的位置
  };

  EventLoop* loop_;
  TcpServer server_;
  int idleSeconds_;
  WeakConnectionList connectionList_; // 连接列表，按照最后一次收到消息的时间先后来排序
};

EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int idleSeconds)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer"),
    idleSeconds_(idleSeconds)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
  
  // 注册一个1s定时器
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));
  dumpConnectionList();
}

void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    // 连接所对应的节点
    Node node;
    node.lastReceiveTime = Timestamp::now();
    connectionList_.push_back(conn);
    node.position = --connectionList_.end();//最后一个元素的位置
    conn->setContext(node);	// 将TcpConnection与Node关联，以便得到conn，就能得到node
  }
  else
  {
    assert(!conn->getContext().empty());
    const Node& node = boost::any_cast<const Node&>(conn->getContext());//连接断开，得到node
    connectionList_.erase(node.position);//从连接列表中移除
  }
  dumpConnectionList();
}

void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size()
           << " bytes at " << time.toString();
  conn->send(msg);

  assert(!conn->getContext().empty());

  Node* node = boost::any_cast<Node>(conn->getMutableContext());
  node->lastReceiveTime = time;
  // move node inside list with list::splice()
  // 时间更新了，需要将连接移至列表末端，以保证列表是按最后更新时刻排序
  connectionList_.erase(node->position);
  connectionList_.push_back(conn);
  node->position = --connectionList_.end();

  dumpConnectionList();
}

void EchoServer::onTimer()
{
  dumpConnectionList();
  Timestamp now = Timestamp::now();
  // 遍历连接列表
  for (WeakConnectionList::iterator it = connectionList_.begin();
      it != connectionList_.end();)
  {
    TcpConnectionPtr conn = it->lock();
    /*
    若超时时间是8s
    connectionList_有：11:28:01 03 05 07 11 12 13 17

    当前的时间是19，则遍历到11的时候，就会break出来，后面都没超时
    遇到第一个没有超时的，就全部break循环出来
    */
    if (conn)
    {
      // 取出连接所对应的节点
      Node* n = boost::any_cast<Node>(conn->getMutableContext());
      double age = timeDifference(now, n->lastReceiveTime);
      if (age > idleSeconds_) // 说明超时了
      {
        conn->shutdown();
      }
      else if (age < 0) // 这种情况一般不可能发生
      {
        LOG_WARN << "Time jump";
        n->lastReceiveTime = now;
      }
      else // age >= 0且age <= idleSeconds_，说明未超时
      {
        break;
      }
      ++it;
    }
    else // 说明连接已关闭，若conn提升失败
    {
      LOG_WARN << "Expired";
      it = connectionList_.erase(it);
    }
  }
}

void EchoServer::dumpConnectionList() const
{
  LOG_INFO << "size = " << connectionList_.size();

  for (WeakConnectionList::const_iterator it = connectionList_.begin();
      it != connectionList_.end(); ++it)
  {
    TcpConnectionPtr conn = it->lock();
    if (conn)
    {
      printf("conn %p\n", get_pointer(conn));
      const Node& n = boost::any_cast<const Node&>(conn->getContext());
      printf("    time %s\n", n.lastReceiveTime.toString().c_str());
    }
    else
    {
      printf("expired\n");
    }
  }
}
/*
优化思路:每隔一秒钟，执行以下的代码。遍历整个连接列表，判断是否超时。
若连接成千上万的话，遍历时间是很长的。所以若按照时间排序的，那么只需要遍历超时的那一部分，剩余的不超时的一部分就不需要遍历了。

其实是对（1）粗暴方法1的优化
*/
int main(int argc, char* argv[])
{
  EventLoop loop;
  InetAddress listenAddr(2007);
  int idleSeconds = 10;
  if (argc > 1)
  {
    idleSeconds = atoi(argv[1]);
  }
  LOG_INFO << "pid = " << getpid() << ", idle seconds = " << idleSeconds;
  EchoServer server(&loop, listenAddr, idleSeconds);
  server.start();
  loop.loop();
}

