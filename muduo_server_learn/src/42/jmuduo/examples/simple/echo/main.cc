#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  muduo::net::EventLoop loop;//����һ��EventLoop����
  muduo::net::InetAddress listenAddr(2007);//����һ����ַ����
  EchoServer server(&loop, listenAddr);//����һ������������
  server.start();
  loop.loop();
}

