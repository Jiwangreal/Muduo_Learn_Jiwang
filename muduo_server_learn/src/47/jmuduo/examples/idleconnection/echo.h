#ifndef MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
#define MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H

#include <muduo/net/TcpServer.h>
//#include <muduo/base/Types.h>

#include <boost/circular_buffer.hpp>
#include <boost/unordered_set.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < 104700
namespace boost
{
template <typename T>
inline size_t hash_value(const boost::shared_ptr<T>& x)
{
  return boost::hash_value(x.get());
}
}
#endif

// RFC 862
class EchoServer
{
 public:
  EchoServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr,
             int idleSeconds);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  void onTimer();

  void dumpConnectionBuckets() const;

  typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;

  struct Entry : public muduo::copyable
  {
    // 因为是弱引用，构造的时候弱引用的引用计数不会+1
    explicit Entry(const WeakTcpConnectionPtr& weakConn)
      : weakConn_(weakConn)
    {
    }

    ~Entry()
    {
      // 释放时，弱引用提升一下，看下能否提升，弱提升成功，则说明弱引用存在
      //找到TcpConnectionPtr所对应的连接，将其断开
      muduo::net::TcpConnectionPtr conn = weakConn_.lock();
      if (conn)
      {
        conn->shutdown();
      }
    }

    WeakTcpConnectionPtr weakConn_;//连接的弱引用
  };
  // 每一个元素是一个Entryshared_ptr的引用计数减为0的时候，Entry的析构函数才会被调用
  typedef boost::shared_ptr<Entry> EntryPtr;  // set中的元素是一个EntryPtr
  typedef boost::weak_ptr<Entry> WeakEntryPtr;

  /*
  set：当我们把元素存入set的时候，会自动排序
  unordered_set不需要排序，效率会提升
  */
//  set中的每一个元素应该是使用带引用计数的智能指针来管理EntryPtr
  typedef boost::unordered_set<EntryPtr> Bucket;  // 环形缓冲区每个格子存放的是一个hash_set

  // 环形队列，环形缓冲区的每一个阁子是一个set集合
  typedef boost::circular_buffer<Bucket> WeakConnectionList;  // 环形缓冲区

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  WeakConnectionList connectionBuckets_; // 连接队列环形缓冲区
};

#endif  // MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
