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
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this)); // 注册一个1s定时器
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
    // 新建一个EntryPtr对象与conn相关联
    EntryPtr entry(new Entry(conn));
    connectionBuckets_.back().insert(entry);  // 插入到队尾，这时候引用计数为2
    dumpConnectionBuckets();
    WeakEntryPtr weakEntry(entry);
    WeakEntryPtr weakEntry(entry);
    // 弱引用与conn相关联，弱引用的话，引用计数不会+1
    conn->setContext(weakEntry);
  }
  else
  {
    assert(!conn->getContext().empty());
    // 取出弱引用
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
    // 打印引用计数
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
  EntryPtr entry(weakEntry.lock());//提升成shared_ptr
  // 用下面这行代替上面2行会如何？这里不能new一个entry，虽然引用计数=1，但是与以前的TcpConnectionPtr就没有关联关系了
  // EntryPtr entry(new Entry(conn));
  if (entry)
  {
    /*
    优化点：队尾，每隔1s钟向前移动一格，若在1s钟之内，队尾bucket中的连接收到消息，这个时候是不需要增加所对应的entry的。
    在现在的实现中，每次收到消息都会往队尾添加 EntryPtr （当然， hash set 会帮我们去重。）
    进一步可以使用排序链表，按时间来排序47\jmuduo\examples\idleconnection\sortedlist.cc
    */
    connectionBuckets_.back().insert(entry);//使得其引用计数+1而已
    dumpConnectionBuckets();//打印桶的连接数
  }
}

void EchoServer::onTimer()
{
  // 整个桶bucket是一个集合，相当于把整个集合给弹出去了，相当于将tail位置原有的Bucket删除了，然后增加了一个空的Bucket
  // 在tail位置插入一个桶，就意味着原来桶的内容都被销毁了，而桶里面存放的是set，而set里面存放的是shared_ptr，那么
  // 这些Entry的引用计数都减1，一旦引用计数减为0，则Entry的析构函数会被调用，会断开连接
  // tail向前移动
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

