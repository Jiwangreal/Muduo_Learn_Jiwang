#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void runInThread()
{
  printf("runInThread(): pid = %d, tid = %d\n",
         getpid(), CurrentThread::tid());//这俩值是不一样的
}

int main()
{
  // 主线程
  printf("main(): pid = %d, tid = %d\n",
         getpid(), CurrentThread::tid());//这俩值应该是一样的，因为这是线程的真实id

  // 构造一个IO线程对象，但是IO线程还未启动，并不代表OS上的IO线程
  EventLoopThread loopThread;

  // 创建IO线程，loop指针指向了IO线程中的栈上对象EventLoop loop;，因为threadFunc()线程还没结束，该loop对象还存在
  EventLoop* loop = loopThread.startLoop();

  // runInLoop跨线程调用
  // 在主线程中调用runInLoop()，而不是在该loop对象所在的线程中调用runInLoop()
  // 这就是异步调用runInThread，即将runInThread添加到loop对象所在IO线程的队列中，IO线程的poll会唤醒，接着会执行该函数，让该IO线程执行
  // runInThread()应该在loop所在的IO线程中调用
  loop->runInLoop(runInThread);//将回调函数传递进去会异步调用
  sleep(1);

  //runAfter跨线程调用
  // runAfter内部也调用了runInLoop，所以这里也是异步调用：让loop对象所在的IO线程去调用runInThread
  loop->runAfter(2, runInThread);
  sleep(3);
  loop->quit();

  printf("exit main().\n");
}