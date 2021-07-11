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
    // ���ӽ������Ͽ����ص�
    client_.setConnectionCallback(
        boost::bind(&ChatClient::onConnection, this, _1));
    
    // �������Ϣ����ʱ�ص�����IO�߳��У���onMessage�е�messageCallback_�ص�onStringMessage
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

  // �ú��������߳���ִ��
  void write(const StringPiece& message)
  {
    // mutex��������connection_���shared_ptr
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      codec_.send(get_pointer(connection_), message);
    }
  }

 private:
  // �ú�����IO�߳���ִ�У�IO�߳������̲߳���ͬһ���߳�
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    // mutex��������connection_���shared_ptr����ΪIO�̺߳����̶߳�Ҫ����connection_
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

// 2���߳�
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 2)
  {
    EventLoopThread loopThread;
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    InetAddress serverAddr(argv[1], port);//����һ����ַ

    // ��IO�̴߳���ͻ��ˣ�IO�̻߳����Խ��շ�������ת������������
    ChatClient client(loopThread.startLoop(), serverAddr);
    client.connect();

    // �Ӽ��̻�ȡ���ݣ�һ�����̣߳����ڼ������룬���ݷ��͸���������
    std::string line;
    while (std::getline(std::cin, line))
    {
      client.write(line);
    }
    client.disconnect();
  }
  else
  {
    // �����ip+�˿ں�
    printf("Usage: %s host_ip port\n", argv[0]);
  }
}

