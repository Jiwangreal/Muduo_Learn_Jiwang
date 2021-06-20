#include <muduo/net/EventLoop.h>
//#include <muduo/net/EventLoopThread.h>
//#include <muduo/base/Thread.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;
int g_flag = 0;

void run4()
{
  printf("run4(): pid = %d, flag = %d\n", getpid(), g_flag);
  g_loop->quit();//loop就退出来了
}

void run3()
{
  printf("run3(): pid = %d, flag = %d\n", getpid(), g_flag);
  g_loop->runAfter(3, run4);
  g_flag = 3;
}

void run2()
{
  printf("run2(): pid = %d, flag = %d\n", getpid(), g_flag);
  g_loop->queueInLoop(run3);//将回调任务run3()添加到队列中
}

void run1()
{
  g_flag = 1;
  printf("run1(): pid = %d, flag = %d\n", getpid(), g_flag);
  // IO线程中调用run2()，是不会放到队列中，而是直接执行
  g_loop->runInLoop(run2);
  g_flag = 2;
}

// 单IO线程调用的eg
int main()
{
  printf("main(): pid = %d, flag = %d\n", getpid(), g_flag);

  EventLoop loop;
  g_loop = &loop;

  // runAfter可以跨线程调用，因为它使用了addTimer，而addTimer使用了runInLoop
  loop.runAfter(2, run1);//2s后调用run1
  loop.loop();
  printf("main(): pid = %d, flag = %d\n", getpid(), g_flag);
}
