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
  g_loop->quit();//loop���˳�����
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
  g_loop->queueInLoop(run3);//���ص�����run3()��ӵ�������
}

void run1()
{
  g_flag = 1;
  printf("run1(): pid = %d, flag = %d\n", getpid(), g_flag);
  // IO�߳��е���run2()���ǲ���ŵ������У�����ֱ��ִ��
  g_loop->runInLoop(run2);
  g_flag = 2;
}

// ��IO�̵߳��õ�eg
int main()
{
  printf("main(): pid = %d, flag = %d\n", getpid(), g_flag);

  EventLoop loop;
  g_loop = &loop;

  // runAfter���Կ��̵߳��ã���Ϊ��ʹ����addTimer����addTimerʹ����runInLoop
  loop.runAfter(2, run1);//2s�����run1
  loop.loop();
  printf("main(): pid = %d, flag = %d\n", getpid(), g_flag);
}
