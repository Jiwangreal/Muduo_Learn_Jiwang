#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

const size_t frameLen = 2*sizeof(int64_t);//16字节

void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    conn->setTcpNoDelay(true);//减少误差，在tcp层一有数据到来则发送回去
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
  //  message[0]是客户端上的时间T1 ，message[1]是时刻T2
    message[1] = receiveTime.microSecondsSinceEpoch();
    conn->send(message, sizeof message);
  }
}

// 时间同步的服务端
void runServer(uint16_t port)
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(port), "ClockServer");

  // 连接到来或者关闭的回调
  server.setConnectionCallback(serverConnectionCallback);
  // 消息到来回调
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
    // 客户端的T1
    int64_t send = message[0];
    // 服务器端的T2
    int64_t their = message[1];
    // T3时间
    int64_t back = receiveTime.microSecondsSinceEpoch();
    int64_t mine = (back+send)/2;
    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;//延迟，服务器端与客户端的时间差
  }
}

void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
    // 获取T1时间
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
      // 以服务器端方式运行
      runServer(port);
    }
    else
    {
      // 以客户端方式运行
      runClient(argv[1], port);
    }
  }
  else
  {
    printf("Usage:\n%s -s port\n%s ip port\n", argv[0], argv[0]);
  }
}

