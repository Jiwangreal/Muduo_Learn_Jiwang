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
    // 设置文件缓冲区
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
  // 打印当前应用层缓冲区大小
  LOG_INFO << "HighWaterMark " << len;
}

void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

   //一旦连接建立  
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();

    // 设置高水位标回调函数，当应用层的发送缓冲区OutBuffer超过64K，回调onHighWaterMark
    conn->setHighWaterMarkCallback(onHighWaterMark, 64*1024);
    // 读取g_file到fileContent
    // 若g_file比较大，那么fileContent也就比较大，会占用很大的内存，一个连接占用1G内存，则多个连接则会占用多个1G内存
    // 导致占用内存过大
    string fileContent = readFile(g_file);
    /*
    send函数是非阻塞的，立即返回。不用担心数据什么时候给对等端，这个由网络库muduo负责到底。
    fileContent比较大的时候，是没有办法一次性将数据拷贝到内核缓冲区的，这时候，会将剩余的数据拷贝到应用层的OutputBuffer中。
    当内核缓冲区中的数据发送出去之后，可写事件产生，muduo就会从OutBuffer中取出数据发送。

    conn->send(fileContent);直接conn->shutdown();是否有问题？没有问题
    shutdown内部实现仅仅只是关闭写入这一半，shutdown——>shutdownInLoop();当前不处于发送数据的状态，才会调用socket_——>shutdownWrite();
    等所有的数据发送完毕之后，才关闭写入这一半，这是通过状态的方式实现的
    数据没发送完毕，仅将状态置为kDisconnecting，同时，一旦内核缓冲区中的数据发送出去之后，可写事件产生，则回调handleWrite，
    if (outputBuffer_.readableBytes() == 0)	时，才会真正的关闭写的一半。
    也就是说muduo网络库会等fileContent所有数据发送完毕，才会真正的关闭写的一半。

    */
    conn->send(fileContent);
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}

/*
一次性把文件读入内存，一次性调用 send(const string&) 发送完毕
缺点：
内存消耗跟文件大小有关
*/
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    // 要下载的文件名称
    g_file = argv[1];

    // TCP服务器
    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);//连接建立回调onConnection
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

