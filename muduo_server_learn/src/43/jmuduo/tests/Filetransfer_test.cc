#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/TcpClient.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <assert.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

AtomicInt32 g_aliveConnections;
AtomicInt32 g_disaliveConnections;
int g_connections = 4;
EventLoop* g_loop;

class RecvFileClient : boost::noncopyable
{
 public:
  RecvFileClient(EventLoop* loop, const InetAddress& serverAddr, const string& id)
    : loop_(loop),
      client_(loop, serverAddr, "RecvFileClient")
  {
    client_.setConnectionCallback(
        boost::bind(&RecvFileClient::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&RecvFileClient::onMessage, this, _1, _2, _3));
    string filename = "RecvFileClient" + id;//文件名称
    fp_ = ::fopen(filename.c_str(), "we");
    assert(fp_);
  }

  ~RecvFileClient()
  {
    ::fclose(fp_);
  }

  void connect()
  {
    client_.connect();
  }

 private:
//  一旦连接建立，回调
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
      connection_ = conn;
      // 多线程，使用原子对象，保护全局变量
      if (g_aliveConnections.incrementAndGet() == g_connections)//所有连接都建立了
        LOG_INFO << "all connected";
    }
    else
    {
      // TcpConnectionPtr使其销毁掉，引用计数减为0
      connection_.reset();
	  
      if (g_disaliveConnections.incrementAndGet() == g_connections)//所有连接对象销毁
      {
        LOG_INFO << "all disconnected";
        // g_loop->quit();//会使得主线程的loop.loop();跳出来，使得程序结束
        exit(0);
      }
    }
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    // 消息到达时，将文件内容写入到fp_
    fwrite(buf->peek(), 1, buf->readableBytes(), fp_);
	  buf->retrieveAll();
  }

  EventLoop* loop_;
  TcpClient client_;
  TcpConnectionPtr connection_;
  FILE* fp_;//打开的文件指针
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  g_loop = &loop;

  // 压力测试程序：用两个IO线程来发起大量的连接
  EventLoopThreadPool loopPool(&loop);//IO线程池
  loopPool.setThreadNum(2);
  loopPool.start();

  // RecvFileClient是TcpClient的包装，一旦连接建立，就获得一个TcpConnectionPtr对象
  boost::ptr_vector<RecvFileClient> clients(g_connections);

  InetAddress serverAddr("127.0.0.1", 2021);

  // 2个线程处理8个连接
  for (int i = 0; i < g_connections; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "%d", i+1);
    // loopPool.getNextLoop()：选择一个EventLoop对象，也就是选择了EventLoop对象所属的IO线程来处理客户端的下载文件
    clients.push_back(new RecvFileClient(loopPool.getNextLoop(), serverAddr, buf));
    clients[i].connect();//发起连接
    usleep(200);
  }

  loop.loop();
  usleep(20000);
}


