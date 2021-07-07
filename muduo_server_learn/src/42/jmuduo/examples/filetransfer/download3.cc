#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/shared_ptr.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
  LOG_INFO << "HighWaterMark " << len;
}

const int kBufSize = 64*1024;
const char* g_file = NULL;

// 共享的智能指针，用来管理FILE*
typedef boost::shared_ptr<FILE> FilePtr;

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
      // 通常shared_ptr接收一个指针参数，这里的意思是：这里fp并不是一个类的指针，而是一个单纯的指针
      // 类的指针：表示ctx引用计数减为0的时候，销毁ctx，会调用delete，进而调用类指针的析构函数
      // 单纯的指针：表示ctx引用计数减为0的时候，要销毁fp是通过fclose()来销毁的
      FilePtr ctx(fp, ::fclose);
      conn->setContext(ctx);//将智能指针与TcpConnection对象绑定，则fp的生存期与TcpConnection对象的生存期就一致了
                            // 智能指针的引用计数=2，离开{}时为1，当TcpConnection对象销毁时，智能指针的引用计数=0，才会去销毁fp，就会自动调用fclose
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
}

void onWriteComplete(const TcpConnectionPtr& conn)
{
  const FilePtr& fp = boost::any_cast<const FilePtr&>(conn->getContext());
  char buf[kBufSize];
  size_t nread = ::fread(buf, 1, sizeof buf, get_pointer(fp));
  if (nread > 0)
  {
    conn->send(buf, nread);
  }
  else
  {
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}
/*
一块一块地发送文件，减少内存使用，用到了 WriteCompleteCallback，这个
版本满足了上述全部健壮性要求。

但是采用 shared_ptr 来管理 FILE*，避免手动调用::fclose(3)。
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
    server.setWriteCompleteCallback(onWriteComplete);
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

