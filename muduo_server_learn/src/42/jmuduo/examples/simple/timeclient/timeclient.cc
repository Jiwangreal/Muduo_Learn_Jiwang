#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>

#include <boost/bind.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class TimeClient : boost::noncopyable
{
 public:
  TimeClient(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      client_(loop, serverAddr, "TimeClient")
  {
    client_.setConnectionCallback(
        boost::bind(&TimeClient::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&TimeClient::onMessage, this, _1, _2, _3));
    // client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

 private:

  EventLoop* loop_;
  TcpClient client_;

  // 一旦连接建立，回调onConnection()
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (!conn->connected())
    {
      loop_->quit();
    }
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
  {
    if (buf->readableBytes() >= sizeof(int32_t))
    {
      // 偷看数据并没取出
      const void* data = buf->peek();
      int32_t be32 = *static_cast<const int32_t*>(data);//得到32位整数的网络字节序

    // 真正从缓冲区取回
      buf->retrieve(sizeof(int32_t));
      time_t time = sockets::networkToHost32(be32);

      // time * Timestamp::kMicroSecondsPerSecond还是32位的整数，但是可能会溢出，超过32位整数所表示的最大的数
      Timestamp ts(static_cast<int64_t>(time) * Timestamp::kMicroSecondsPerSecond);//static_cast<int64_t>(time) 是int64_t的整数
                                                                                  //static_cast<int64_t>(time) * Timestamp::kMicroSecondsPerSecond还是
                                                                                  //int64_t的整数
      LOG_INFO << "Server time = " << time << ", " << ts.toFormattedString();
    }
    else
    {
      LOG_INFO << conn->name() << " no enough data " << buf->readableBytes()
               << " at " << receiveTime.toFormattedString();
    }
  }
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    InetAddress serverAddr(argv[1], 2037);//服务器地址

    TimeClient timeClient(&loop, serverAddr);
    timeClient.connect();//发起连接
    loop.loop();
  }
  else
  {
    printf("Usage: %s host_ip\n", argv[0]);
  }
}

