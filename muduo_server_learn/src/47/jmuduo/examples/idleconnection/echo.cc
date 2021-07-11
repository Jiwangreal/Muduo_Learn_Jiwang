#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include <assert.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int idleSeconds)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer"),
    connectionBuckets_(idleSeconds)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this)); // ע��һ��1s��ʱ��
  connectionBuckets_.resize(idleSeconds);
  dumpConnectionBuckets();
}

void EchoServer::start()
{
  server_.start();
}

void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    // �½�һ��EntryPtr������conn�����
    EntryPtr entry(new Entry(conn));
    connectionBuckets_.back().insert(entry);  // ���뵽��β����ʱ�����ü���Ϊ2
    dumpConnectionBuckets();
    WeakEntryPtr weakEntry(entry);
    WeakEntryPtr weakEntry(entry);
    // ��������conn������������õĻ������ü�������+1
    conn->setContext(weakEntry);
  }
  else
  {
    assert(!conn->getContext().empty());
    // ȡ��������
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
    // ��ӡ���ü���
    LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
  }
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
  WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
  EntryPtr entry(weakEntry.lock());//������shared_ptr
  // ���������д�������2�л���Σ����ﲻ��newһ��entry����Ȼ���ü���=1����������ǰ��TcpConnectionPtr��û�й�����ϵ��
  // EntryPtr entry(new Entry(conn));
  if (entry)
  {
    /*
    �Ż��㣺��β��ÿ��1s����ǰ�ƶ�һ������1s��֮�ڣ���βbucket�е������յ���Ϣ�����ʱ���ǲ���Ҫ��������Ӧ��entry�ġ�
    �����ڵ�ʵ���У�ÿ���յ���Ϣ��������β��� EntryPtr ����Ȼ�� hash set �������ȥ�ء���
    ��һ������ʹ������������ʱ��������47\jmuduo\examples\idleconnection\sortedlist.cc
    */
    connectionBuckets_.back().insert(entry);//ʹ�������ü���+1����
    dumpConnectionBuckets();//��ӡͰ��������
  }
}

void EchoServer::onTimer()
{
  // ����Ͱbucket��һ�����ϣ��൱�ڰ��������ϸ�����ȥ�ˣ��൱�ڽ�tailλ��ԭ�е�Bucketɾ���ˣ�Ȼ��������һ���յ�Bucket
  // ��tailλ�ò���һ��Ͱ������ζ��ԭ��Ͱ�����ݶ��������ˣ���Ͱ�����ŵ���set����set�����ŵ���shared_ptr����ô
  // ��ЩEntry�����ü�������1��һ�����ü�����Ϊ0����Entry�����������ᱻ���ã���Ͽ�����
  // tail��ǰ�ƶ�
  connectionBuckets_.push_back(Bucket());
  dumpConnectionBuckets();
}

void EchoServer::dumpConnectionBuckets() const
{
  LOG_INFO << "size = " << connectionBuckets_.size();
  int idx = 0;
  for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.begin();
      bucketI != connectionBuckets_.end();
      ++bucketI, ++idx)
  {
    const Bucket& bucket = *bucketI;
    printf("[%d] len = %zd : ", idx, bucket.size());
    for (Bucket::const_iterator it = bucket.begin();
        it != bucket.end();
        ++it)
    {
      bool connectionDead = (*it)->weakConn_.expired();
      printf("%p(%ld)%s, ", get_pointer(*it), it->use_count(),
          connectionDead ? " DEAD" : "");
    }
    puts("");
  }
}

