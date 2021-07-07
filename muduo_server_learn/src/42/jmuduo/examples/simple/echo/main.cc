#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  muduo::net::EventLoop loop;//定义一个EventLoop对象
  muduo::net::InetAddress listenAddr(2007);//定义一个地址对象
  EchoServer server(&loop, listenAddr);//定义一个服务器对象
  server.start();
  loop.loop();
}

