#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include <stdio.h>
#include <sys/timerfd.h>

using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;
int timerfd;

//在I/O线程中调用timeout()，接着在I/O线程中调用quit()
void timeout(Timestamp receiveTime)
{
	printf("Timeout!\n");
	uint64_t howmany;
	//把数据读走，否则会一直触发，因为poll是高电平触发的，缓冲区有数据会一直是高电平的
	::read(timerfd, &howmany, sizeof howmany);
	g_loop->quit();
}

int main(void)
{

	//构造时间循环对象
	EventLoop loop;
	g_loop = &loop;

	//创建一个定时器
	timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	//创建一个通道，该通道属于EventLoop
	Channel channel(&loop, timerfd);

	//channel可读事件到来的时候会回调timeout函数，_1表示传递一个参数进来，传递到timeout函数中
	channel.setReadCallback(boost::bind(timeout, _1));

	//enableReading导致update()导致loop_->updateChannel()导致poller->updateChannel()
	channel.enableReading();//关注可读事件，看上面时序图可知最终将channel注册到PollPoller或者EPollPoller
							//PollPoller或者EPollPoller现在可以关注定时器的可读事件了

	//设定定时器
	//设置定时器的超时时间
	struct itimerspec howlong;
	bzero(&howlong, sizeof howlong);//等价于memset(,0,)
									//it_interval=0,说明是一次性的定时器
	howlong.it_value.tv_sec = 1;
	::timerfd_settime(timerfd, 0, &howlong, NULL);
	
	//事件循环
	//定时器1s钟超时了就会产生可读事件，看上面时序图可知最终会调用timeout()
	loop.loop();//loop产生了时事件

	::close(timerfd);
}



