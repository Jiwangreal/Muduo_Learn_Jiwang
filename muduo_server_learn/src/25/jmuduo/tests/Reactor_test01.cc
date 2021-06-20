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

	//���̴߳�����EventLoop����
	EventLoop loop;

	Thread t(threadFunc);//���߳��������߳���ô����
	t.start();

	//���߳�ѭ��ͣ5s
	loop.loop();
	t.join();
	return 0;
}
