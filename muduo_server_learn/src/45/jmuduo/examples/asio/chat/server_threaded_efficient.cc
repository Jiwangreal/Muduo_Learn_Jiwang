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
    connections_(new ConnectionList)//shared_ptr�����ü���=1����connections_�б���2��������һ����������һ��д����
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
//  д��������һ���߳�
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);
    // �����������е��ٽ���
    // connections_�����ʱ�����ü���=1�ˣ���������Ϊ1
    if (!connections_.unique())		// ˵�����ü�������1������ʱ=2
    {
      // new ConnectionList(*connections_)��δ��뿽����һ��ConnectionList
      connections_.reset(new ConnectionList(*connections_));//*connections_����ConnectionList���󣬽�ԭ����connections_�����ݿ������쵽ConnectionList
                                                            //ConnectionList��connections_���ӹܣ�connections_.reset()
                                                            // �µ�connections_���ü���-1=1��ԭ����connections_Ҳ��Ϊ1��
    }
    // �������ü���=1�������ڸ�����ֱ���޸�
    assert(connections_.unique());

    // �ڸ������޸ģ�����Ӱ����ߣ����Զ����ڱ����б��ʱ�򣬲���Ҫ��mutex����
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
  typedef boost::shared_ptr<ConnectionList> ConnectionListPtr;//�������ü���������ָ��

  // ����������һ���߳�
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // ���ü�����1=2��mutex�������ٽ���������̣�����˲����������45\jmuduo\examples\asio\chat\server_threaded.cc
    ConnectionListPtr connections = getConnectionList();//ջ�ϱ���
    // ����Ĵ��벻�ܵ�������
    // ���ܴ�һ������ʣ�����mutex������д�߸����������б���ô�죿
    // ʵ���ϣ�д��������һ���������޸ģ��������赣�ġ�
    // ȱ�㣺���⣬eg��hello��Ϣ�����һ���ͻ��������һ���ͻ���֮����ӳ���Ȼ�Ƚϴ�
    // ��Ϊ������ͻ���ת����Ȼ��ͬһ���߳���ִ�еģ����������ڸ����ͻ�������Ӧ���߳���ִ�У�������Է�װ�����߳��У����߳�ִ��
    // ����45\jmuduo\examples\asio\chat\server_threaded_highperformance.cc
    for (ConnectionList::iterator it = connections->begin();
        it != connections->end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }

	// ������Բ�һ������
	//assert(!connections.unique());
    // ��connections���ջ�ϵı������ٵ�ʱ�����ü�����1=0����д�����������ȥ����������ԭ����connections_�ͻ����ٵ������Բ���������ݸ���
  }

  ConnectionListPtr getConnectionList()
  {
    MutexLockGuard lock(mutex_);
    return connections_;//���ܶ���̷߳���
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  ConnectionListPtr connections_;
};

// �����ǽ���������
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

