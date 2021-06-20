#include <muduo/net/EventLoop.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void threadFunc()
{
	printf("threadFunc(): pid = %d, tid = %d\n",
		getpid(), CurrentThread::tid());

	EventLoop loop;
	loop.loop();
}

int main(void)
{
	printf("main(): pid = %d, tid = %d\n",
		getpid(), CurrentThread::tid());

	//主线程创建了EventLoop对象
	EventLoop loop;

	Thread t(threadFunc);//子线程类似主线程这么操作
	t.start();

	//主线程循环停5s
	loop.loop();
	t.join();
	return 0;
}
