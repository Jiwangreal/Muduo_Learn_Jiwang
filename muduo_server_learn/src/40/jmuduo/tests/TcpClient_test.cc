#include <muduo/net/Channel.h>
#include <muduo/net/TcpClient.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class TestClient
{
 public:
  TestClient(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      client_(loop, listenAddr, "TestClient"),
      stdinChannel_(loop, 0)//构造一个标准输入通道对象stdinChannel_，它所对应的fd=0
  {
    // 连接建立成功，回调onConnection
    client_.setConnectionCallback(
        boost::bind(&TestClient::onConnection, this, _1));
    //消息到达回调onMessage
    client_.setMessageCallback(
        boost::bind(&TestClient::onMessage, this, _1, _2, _3));
    //client_.enableRetry();

    // 标准输入缓冲区中有数据的时候，回调TestClient::handleRead
    stdinChannel_.setReadCallback(boost::bind(&TestClient::handleRead, this));
	stdinChannel_.enableReading();		// 关注可读事件
                                    //当从标准输入输入数据的时候，且按下回车，此时标准输入缓冲区就有数据了
                                    //此时标准输入就处于可读状态，就会回调handleRead
  }

  void connect()
  {
    client_.connect();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    // 连接建立
    if (conn->connected())
    {
      printf("onConnection(): new connection [%s] from %s\n",
             conn->name().c_str(),
             conn->peerAddress().toIpPort().c_str());
    }
    else
    {
      // 连接断开
      printf("onConnection(): connection [%s] is down\n",
             conn->name().c_str());
    }
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    string msg(buf->retrieveAllAsString());
    printf("onMessage(): recv a message [%s]\n", msg.c_str());
    LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << time.toFormattedString();
  }

  // 从键盘上接收的数据，会回调这个函数
  // 标准输入缓冲区中有数据的时候，回调该函数
  void handleRead()
  {
    char buf[1024] = {0};
    fgets(buf, 1024, stdin);
	buf[strlen(buf)-1] = '\0';		// 去除\n
	client_.connection()->send(buf);
  }

  EventLoop* loop_;
  TcpClient client_;//客户端程序
  Channel stdinChannel_;		// 标准输入Channel
};
/*
（1）
从键盘接收输入//前台线程
从网络接收数据//IO线程

（2）该程序使用一个IO线程（标准输入IO+网络IO）来实现的
*/
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  EventLoop loop;
  InetAddress serverAddr("127.0.0.1", 8888);

  // 就算服务端没启动，客户端也可以自动重连，因为Connector具有重连功能
  TestClient client(&loop, serverAddr);
  client.connect();
  loop.loop();
}

