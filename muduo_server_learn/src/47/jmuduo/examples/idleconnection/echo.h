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
    // ��Ϊ�������ã������ʱ�������õ����ü�������+1
    explicit Entry(const WeakTcpConnectionPtr& weakConn)
      : weakConn_(weakConn)
    {
    }

    ~Entry()
    {
      // �ͷ�ʱ������������һ�£������ܷ��������������ɹ�����˵�������ô���
      //�ҵ�TcpConnectionPtr����Ӧ�����ӣ�����Ͽ�
      muduo::net::TcpConnectionPtr conn = weakConn_.lock();
      if (conn)
      {
        conn->shutdown();
      }
    }

    WeakTcpConnectionPtr weakConn_;//���ӵ�������
  };
  // ÿһ��Ԫ����һ��Entryshared_ptr�����ü�����Ϊ0��ʱ��Entry�����������Żᱻ����
  typedef boost::shared_ptr<Entry> EntryPtr;  // set�е�Ԫ����һ��EntryPtr
  typedef boost::weak_ptr<Entry> WeakEntryPtr;

  /*
  set�������ǰ�Ԫ�ش���set��ʱ�򣬻��Զ�����
  unordered_set����Ҫ����Ч�ʻ�����
  */
//  set�е�ÿһ��Ԫ��Ӧ����ʹ�ô����ü���������ָ��������EntryPtr
  typedef boost::unordered_set<EntryPtr> Bucket;  // ���λ�����ÿ�����Ӵ�ŵ���һ��hash_set

  // ���ζ��У����λ�������ÿһ��������һ��set����
  typedef boost::circular_buffer<Bucket> WeakConnectionList;  // ���λ�����

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  WeakConnectionList connectionBuckets_; // ���Ӷ��л��λ�����
};

#endif  // MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
