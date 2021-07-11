#include "BankServer.h"
#include "Server.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  //LOG_INFO << "pid = " << getpid();
  // 事件循环
  muduo::net::EventLoop loop;
  
  // 地址
  muduo::net::InetAddress listenAddr(muduo::Singleton<Server>::instance().GetPort());
  
  BankServer server(&loop, listenAddr);
  
  // 启动
  server.start();
  loop.loop();
}

