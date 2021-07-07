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
      // conn��һ��TcpConnection����ʵ�ʾ��ǽ�TcpConnection��һ������fp����һ��
      // ͨ�����ַ��������ǾͲ���Ҫ��������һ��map�����������Ӧ��ϵ��key=TcpConnection����value=fpָ�룩
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
  else//���ӹر�
  {
    // ������������
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

// �����һ�飬�����ž�Ҫ����2��
void onWriteComplete(const TcpConnectionPtr& conn)
{
  FILE* fp = boost::any_cast<FILE*>(conn->getContext());
  char buf[kBufSize];
  // ����ǰ�汣����fpָ�룬�����ٴζ�ȡ�ͻ��ȡ��һ��������
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
һ��һ��ط����ļ��������ڴ�ʹ�ã��õ��� WriteCompleteCallback�����
�汾����������ȫ����׳��Ҫ��
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
    server.setWriteCompleteCallback(onWriteComplete);//���ݷ�����ϻص�
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

