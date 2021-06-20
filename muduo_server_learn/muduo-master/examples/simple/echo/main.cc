#include "examples/simple/echo/echo.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <unistd.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  //事件循环对象，one loop per thread+threadpool
  //每个线程最多一个EventLoop对象，若没有就不是I/O线程
  muduo::net::EventLoop loop;//EventLoop就是对Reactor模式的封装
  muduo::net::InetAddress listenAddr(2007);
  //若是multiple reactor的话，这个reactor就是mainrreactor，内部还会创建subreactor
  EchoServer server(&loop, listenAddr);
  server.start();//绑定监听等
  loop.loop();//捕捉事件，一旦事件到达，会回调EchoServer中相应函数
}

