#include <muduo/net/inspect/Inspector.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

using namespace muduo;
using namespace muduo::net;

int main()
{
  EventLoop loop;//线程1
  EventLoopThread t;	// 监控线程，线程2
  Inspector ins(t.startLoop(), InetAddress(12345), "test");//端口是12345
                                                            //Inspector对象所属的IO线程应该是t.startLoop()这个IO线程EventLoop
                                                            //而不是主线程的EventLoop loop;
  loop.loop();
}

