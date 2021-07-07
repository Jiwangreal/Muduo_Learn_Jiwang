#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
  LOG_INFO << "HighWaterMark " << len;
}

const int kBufSize = 64*1024;
const char* g_file = NULL;

void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();
    conn->setHighWaterMarkCallback(onHighWaterMark, kBufSize+1);


    FILE* fp = ::fopen(g_file, "rb");
    if (fp)
    {
      // conn是一个TcpConnection对象，实际就是将TcpConnection与一个数据fp绑定在一起
      // 通过这种方法，我们就不需要额外再用一个map容器来管理对应关系（key=TcpConnection对象，value=fp指针）
      conn->setContext(fp);
      char buf[kBufSize];
      size_t nread = ::fread(buf, 1, sizeof buf, fp);
      conn->send(buf, nread);
    }
    else
    {
      conn->shutdown();
      LOG_INFO << "FileServer - no such file";
    }
  }
  else//连接关闭
  {
    // 上下文有数据
    if (!conn->getContext().empty())
    {
      FILE* fp = boost::any_cast<FILE*>(conn->getContext());
      if (fp)
      {
        ::fclose(fp);
      }
    }
  }
}

// 发完第一块，紧接着就要发第2块
void onWriteComplete(const TcpConnectionPtr& conn)
{
  FILE* fp = boost::any_cast<FILE*>(conn->getContext());
  char buf[kBufSize];
  // 由于前面保存了fp指针，所以再次读取就会读取下一块内容了
  size_t nread = ::fread(buf, 1, sizeof buf, fp);
  if (nread > 0)
  {
    conn->send(buf, nread);
  }
  else
  {
    ::fclose(fp);
    fp = NULL;
    conn->setContext(fp);
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}

/*
一块一块地发送文件，减少内存使用，用到了 WriteCompleteCallback，这个
版本满足了上述全部健壮性要求。
*/ 
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    g_file = argv[1];

    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);
    server.setWriteCompleteCallback(onWriteComplete);//数据发送完毕回调
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

