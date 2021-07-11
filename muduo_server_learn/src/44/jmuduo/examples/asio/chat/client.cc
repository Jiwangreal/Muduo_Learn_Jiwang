#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>

#include <iostream>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class ChatClient : boost::noncopyable
{
 public:
  ChatClient(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      client_(loop, serverAddr, "ChatClient"),
      codec_(boost::bind(&ChatClient::onStringMessage, this, _1, _2, _3))
  {
    // 连接建立（断开）回调
    client_.setConnectionCallback(
        boost::bind(&ChatClient::onConnection, this, _1));
    
    // 服务端消息到来时回调，在IO线程中，在onMessage中的messageCallback_回调onStringMessage
    client_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
    client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

  void disconnect()
  {
    // client_.disconnect();
  }

  // 该函数在主线程中执行
  void write(const StringPiece& message)
  {
    // mutex用来保护connection_这个shared_ptr
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      codec_.send(get_pointer(connection_), message);
    }
  }

 private:
  // 该函数在IO线程中执行，IO线程与主线程不在同一个线程
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    // mutex用来保护connection_这个shared_ptr，因为IO线程和主线程都要访问connection_
    MutexLockGuard lock(mutex_);
    if (conn->connected())
    {
      connection_ = conn;
    }
    else
    {
      connection_.reset();
    }
  }

  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    printf("<<< %s\n", message.c_str());
  }

  EventLoop* loop_;
  TcpClient client_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  TcpConnectionPtr connection_;
};

// 2个线程
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 2)
  {
    EventLoopThread loopThread;
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    InetAddress serverAddr(argv[1], port);//构造一个地址

    // 让IO线程处理客户端，IO线程还可以接收服务器端转发过来的数据
    ChatClient client(loopThread.startLoop(), serverAddr);
    client.connect();

    // 从键盘获取数据，一个主线程：用于键盘输入，数据发送给服务器端
    std::string line;
    while (std::getline(std::cin, line))
    {
      client.write(line);
    }
    client.disconnect();
  }
  else
  {
    // 服务端ip+端口号
    printf("Usage: %s host_ip port\n", argv[0]);
  }
}

