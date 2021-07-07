#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

const char* g_file = NULL;

// FIXME: use FileUtil::readFile()
string readFile(const char* filename)
{
  string content;
  FILE* fp = ::fopen(filename, "rb");
  if (fp)
  {
    // inefficient!!!
    const int kBufSize = 1024*1024;
    char iobuf[kBufSize];
    // �����ļ�������
    ::setbuffer(fp, iobuf, sizeof iobuf);

    char buf[kBufSize];
    size_t nread = 0;
    while ( (nread = ::fread(buf, 1, sizeof buf, fp)) > 0)
    {
      content.append(buf, nread);
    }
    ::fclose(fp);
  }
  return content;
}

void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
  // ��ӡ��ǰӦ�ò㻺������С
  LOG_INFO << "HighWaterMark " << len;
}

void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

   //һ�����ӽ���  
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();

    // ���ø�ˮλ��ص���������Ӧ�ò�ķ��ͻ�����OutBuffer����64K���ص�onHighWaterMark
    conn->setHighWaterMarkCallback(onHighWaterMark, 64*1024);
    // ��ȡg_file��fileContent
    // ��g_file�Ƚϴ���ôfileContentҲ�ͱȽϴ󣬻�ռ�úܴ���ڴ棬һ������ռ��1G�ڴ棬�����������ռ�ö��1G�ڴ�
    // ����ռ���ڴ����
    string fileContent = readFile(g_file);
    /*
    send�����Ƿ������ģ��������ء����õ�������ʲôʱ����Եȶˣ�����������muduo���𵽵ס�
    fileContent�Ƚϴ��ʱ����û�а취һ���Խ����ݿ������ں˻������ģ���ʱ�򣬻Ὣʣ������ݿ�����Ӧ�ò��OutputBuffer�С�
    ���ں˻������е����ݷ��ͳ�ȥ֮�󣬿�д�¼�������muduo�ͻ��OutBuffer��ȡ�����ݷ��͡�

    conn->send(fileContent);ֱ��conn->shutdown();�Ƿ������⣿û������
    shutdown�ڲ�ʵ�ֽ���ֻ�ǹر�д����һ�룬shutdown����>shutdownInLoop();��ǰ�����ڷ������ݵ�״̬���Ż����socket_����>shutdownWrite();
    �����е����ݷ������֮�󣬲Źر�д����һ�룬����ͨ��״̬�ķ�ʽʵ�ֵ�
    ����û������ϣ�����״̬��ΪkDisconnecting��ͬʱ��һ���ں˻������е����ݷ��ͳ�ȥ֮�󣬿�д�¼���������ص�handleWrite��
    if (outputBuffer_.readableBytes() == 0)	ʱ���Ż������Ĺر�д��һ�롣
    Ҳ����˵muduo�������fileContent�������ݷ�����ϣ��Ż������Ĺر�д��һ�롣

    */
    conn->send(fileContent);
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}

/*
һ���԰��ļ������ڴ棬һ���Ե��� send(const string&) �������
ȱ�㣺
�ڴ����ĸ��ļ���С�й�
*/
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    // Ҫ���ص��ļ�����
    g_file = argv[1];

    // TCP������
    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);//���ӽ����ص�onConnection
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

