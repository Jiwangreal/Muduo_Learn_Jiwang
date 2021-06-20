#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include <stdio.h>
#include <sys/timerfd.h>

using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;
int timerfd;

//��I/O�߳��е���timeout()��������I/O�߳��е���quit()
void timeout(Timestamp receiveTime)
{
	printf("Timeout!\n");
	uint64_t howmany;
	//�����ݶ��ߣ������һֱ��������Ϊpoll�Ǹߵ�ƽ�����ģ������������ݻ�һֱ�Ǹߵ�ƽ��
	::read(timerfd, &howmany, sizeof howmany);
	g_loop->quit();
}

int main(void)
{

	//����ʱ��ѭ������
	EventLoop loop;
	g_loop = &loop;

	//����һ����ʱ��
	timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	//����һ��ͨ������ͨ������EventLoop
	Channel channel(&loop, timerfd);

	//channel�ɶ��¼�������ʱ���ص�timeout������_1��ʾ����һ���������������ݵ�timeout������
	channel.setReadCallback(boost::bind(timeout, _1));

	//enableReading����update()����loop_->updateChannel()����poller->updateChannel()
	channel.enableReading();//��ע�ɶ��¼���������ʱ��ͼ��֪���ս�channelע�ᵽPollPoller����EPollPoller
							//PollPoller����EPollPoller���ڿ��Թ�ע��ʱ���Ŀɶ��¼���

	//�趨��ʱ��
	//���ö�ʱ���ĳ�ʱʱ��
	struct itimerspec howlong;
	bzero(&howlong, sizeof howlong);//�ȼ���memset(,0,)
									//it_interval=0,˵����һ���ԵĶ�ʱ��
	howlong.it_value.tv_sec = 1;
	::timerfd_settime(timerfd, 0, &howlong, NULL);
	
	//�¼�ѭ��
	//��ʱ��1s�ӳ�ʱ�˾ͻ�����ɶ��¼���������ʱ��ͼ��֪���ջ����timeout()
	loop.loop();//loop������ʱ�¼�

	::close(timerfd);
}



