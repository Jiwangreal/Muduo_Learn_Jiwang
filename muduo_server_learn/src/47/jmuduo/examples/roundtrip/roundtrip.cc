#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

const size_t frameLen = 2*sizeof(int64_t);//16�ֽ�

void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    conn->setTcpNoDelay(true);//��������tcp��һ�����ݵ������ͻ�ȥ
  }
  else
  {
  }
}

void serverMessageCallback(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
  //  message[0]�ǿͻ����ϵ�ʱ��T1 ��message[1]��ʱ��T2
    message[1] = receiveTime.microSecondsSinceEpoch();
    conn->send(message, sizeof message);
  }
}

// ʱ��ͬ���ķ����
void runServer(uint16_t port)
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(port), "ClockServer");

  // ���ӵ������߹رյĻص�
  server.setConnectionCallback(serverConnectionCallback);
  // ��Ϣ�����ص�
  server.setMessageCallback(serverMessageCallback);
  server.start();
  loop.loop();
}

TcpConnectionPtr clientConnection;

void clientConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    clientConnection = conn;
    conn->setTcpNoDelay(true);
  }
  else
  {
    clientConnection.reset();
  }
}

void clientMessageCallback(const TcpConnectionPtr&,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
    // �ͻ��˵�T1
    int64_t send = message[0];
    // �������˵�T2
    int64_t their = message[1];
    // T3ʱ��
    int64_t back = receiveTime.microSecondsSinceEpoch();
    int64_t mine = (back+send)/2;
    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;//�ӳ٣�����������ͻ��˵�ʱ���
  }
}

void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
    // ��ȡT1ʱ��
    message[0] = Timestamp::now().microSecondsSinceEpoch();
    clientConnection->send(message, sizeof message);
  }
}

void runClient(const char* ip, uint16_t port)
{
  EventLoop loop;
  TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
  client.enableRetry();
  client.setConnectionCallback(clientConnectionCallback);
  client.setMessageCallback(clientMessageCallback);
  client.connect();
  loop.runEvery(0.2, sendMyTime);
  loop.loop();
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    if (strcmp(argv[1], "-s") == 0)
    {
      // �Է������˷�ʽ����
      runServer(port);
    }
    else
    {
      // �Կͻ��˷�ʽ����
      runClient(argv[1], port);
    }
  }
  else
  {
    printf("Usage:\n%s -s port\n%s ip port\n", argv[0], argv[0]);
  }
}

