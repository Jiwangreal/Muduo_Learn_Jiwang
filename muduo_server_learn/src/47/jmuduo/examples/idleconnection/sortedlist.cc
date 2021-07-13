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
    Timestamp lastReceiveTime; // ���������һ�λ�Ծʱ�̣����һ���յ���Ϣ��ʱ��
    WeakConnectionList::iterator position; // �������������б��е�λ��
  };

  EventLoop* loop_;
  TcpServer server_;
  int idleSeconds_;
  WeakConnectionList connectionList_; // �����б��������һ���յ���Ϣ��ʱ���Ⱥ�������
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
  
  // ע��һ��1s��ʱ��
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
    // ��������Ӧ�Ľڵ�
    Node node;
    node.lastReceiveTime = Timestamp::now();
    connectionList_.push_back(conn);
    node.position = --connectionList_.end();//���һ��Ԫ�ص�λ��
    conn->setContext(node);	// ��TcpConnection��Node�������Ա�õ�conn�����ܵõ�node
  }
  else
  {
    assert(!conn->getContext().empty());
    const Node& node = boost::any_cast<const Node&>(conn->getContext());//���ӶϿ����õ�node
    connectionList_.erase(node.position);//�������б����Ƴ�
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
  // ʱ������ˣ���Ҫ�����������б�ĩ�ˣ��Ա�֤�б��ǰ�������ʱ������
  connectionList_.erase(node->position);
  connectionList_.push_back(conn);
  node->position = --connectionList_.end();

  dumpConnectionList();
}

void EchoServer::onTimer()
{
  dumpConnectionList();
  Timestamp now = Timestamp::now();
  // ���������б�
  for (WeakConnectionList::iterator it = connectionList_.begin();
      it != connectionList_.end();)
  {
    TcpConnectionPtr conn = it->lock();
    /*
    ����ʱʱ����8s
    connectionList_�У�11:28:01 03 05 07 11 12 13 17

    ��ǰ��ʱ����19���������11��ʱ�򣬾ͻ�break���������涼û��ʱ
    ������һ��û�г�ʱ�ģ���ȫ��breakѭ������
    */
    if (conn)
    {
      // ȡ����������Ӧ�Ľڵ�
      Node* n = boost::any_cast<Node>(conn->getMutableContext());
      double age = timeDifference(now, n->lastReceiveTime);
      if (age > idleSeconds_) // ˵����ʱ��
      {
        conn->shutdown();
      }
      else if (age < 0) // �������һ�㲻���ܷ���
      {
        LOG_WARN << "Time jump";
        n->lastReceiveTime = now;
      }
      else // age >= 0��age <= idleSeconds_��˵��δ��ʱ
      {
        break;
      }
      ++it;
    }
    else // ˵�������ѹرգ���conn����ʧ��
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
�Ż�˼·:ÿ��һ���ӣ�ִ�����µĴ��롣�������������б��ж��Ƿ�ʱ��
�����ӳ�ǧ����Ļ�������ʱ���Ǻܳ��ġ�����������ʱ������ģ���ôֻ��Ҫ������ʱ����һ���֣�ʣ��Ĳ���ʱ��һ���־Ͳ���Ҫ�����ˡ�

��ʵ�Ƕԣ�1���ֱ�����1���Ż�
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

