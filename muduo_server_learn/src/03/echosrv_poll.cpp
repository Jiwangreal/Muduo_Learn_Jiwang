#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <vector>
#include <iostream>

#define ERR_EXIT(m) \
        do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)

//向量（动态数组）：向量中的空间是连续的，可以当作数组来用
typedef std::vector<struct pollfd> PollFdList;

int main(void)
{
	//SIGPIPE产生：
	//在Linux网络编程中，12讲，11种状态
	//如果客户端关闭套接字close，而服务端调用了一次write，服务器会接收一个RST segment(在TCO传输层)
	//如果服务端再次调用了write，这个时候就会产生SIGPIPE信号
	//如果没有忽略该信号的话，该信号的默认处理方式就是退出整个进程
	//高可用的服务器需要7*24服务，所以需要忽略掉它

	//TIME_WAIT状态对大并发服务器的影响
	//应尽可能在服务器端避免出现TIME_WAIT状态
	//如果服务器端主动断开连接（先与client调用close），服务端就会进入TIME_WAIT，这样的话，在内核中，会在一定时间内hold住内核资源，
	//使得并发能力大大降低了，所以要避免time_wait状态
	//如何处理TIME_WAIT状态？
	//协议设计上，应该让客户端主动断开连接，这样就把TIME_WAIT状态分散到大量的客户端。但是如果客户端不活跃了，
	//一些客户端不断开连接，这样就会占用服务器端连接的资源。此外，服务端也要有个机制来踢掉不活跃的连接（即调用close，少量没事）
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);//避免僵死进程

	//int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	int listenfd;

	//nonblocking socket +IO多路复用
	//SOCK_NONBLOCK：非阻塞状态
	//SOCK_CLOEXEC：进程被替换时，fd处于关闭状态，eg：fork一个进程又把它替换掉，此时fd是关闭的
	//if ((listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	if ((listenfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)) < 0)
		ERR_EXIT("socket");

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5188);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int on = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ERR_EXIT("setsockopt");

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind");
	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen");

	struct pollfd pfd;
	pfd.fd = listenfd;
	pfd.events = POLLIN;//关注的是POLLIN事件，有数据可读，man poll

	PollFdList pollfds;
	pollfds.push_back(pfd);//关心listenfd的可读事件

	int nready;

	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	while (1)
	{
		//pollfds.begin()：动态数组的首地址，第一个迭代值：*pollfds.begin()
		//第一个迭代值的首地址：&*pollfds.begin()，在C++11中，可以写成：pollfds.data()
		//pollfds.size()：数组的个数，fd的个数，vector向量的大小，
		//-1：无限制等待
		nready = poll(&*pollfds.begin(), pollfds.size(), -1);

		//最下面第二个的continue完毕表面：pollfds数组中有2个fd：listenfd，connfd
		//接下来，listenfd可能产生了可读事件，connfd也可能产生了可读事件
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			
			ERR_EXIT("poll");
		}
		if (nready == 0)	// nothing happended
			continue;
		
		//pollfds数组中第一个fd总是监听fd
		if (pollfds[0].revents & POLLIN)
		{
			peerlen = sizeof(peeraddr);
			/*
			C++表示使用全局的函数: ::accept4。没有同名的，则无所谓加::
			connfd = ::accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
			*/
			//accept函数没有：SOCK_NONBLOCK | SOCK_CLOEXEC等flag
			connfd = accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

			if (connfd == -1)
				ERR_EXIT("accept4");

/*
			if (connfd == -1)
			{
				if (errno == EMFILE)
				{
					close(idlefd);
					idlefd = accept(listenfd, NULL, NULL);
					close(idlefd);
					idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
					continue;
				}
				else
					ERR_EXIT("accept4");
			}
*/

			pfd.fd = connfd;
			pfd.events = POLLIN;
			pfd.revents = 0;//当前只是加入listen，没有任何事件返回
			pollfds.push_back(pfd);
			--nready;

			// 连接成功
			std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<
				" port="<<ntohs(peeraddr.sin_port)<<std::endl;
			if (nready == 0)//说明事件都处理完了
				continue;
				
		}

		//std::cout<<pollfds.size()<<std::endl;
		//std::cout<<nready<<std::endl;

		//遍历哪几个connfd产生了可读事件
		//pollfds.begin()+1开始是，已连接fd
		for (PollFdList::iterator it=pollfds.begin()+1;
			it != pollfds.end() && nready >0; ++it)
		{
				if (it->revents & POLLIN)//返回的事件具有可读事件
				{
					--nready;
					connfd = it->fd;//取出已连接fd
					char buf[1024] = {0};
					int ret = read(connfd, buf, 1024);
					if (ret == -1)
						ERR_EXIT("read");
					if (ret == 0)//等于0，说明对方关闭了socket
					{
						std::cout<<"client close"<<std::endl;
						//将已连接fd从向量vector中移除
						it = pollfds.erase(it);//为什么用向量vector，不用数组？
						--it;//为什么要--it？

						close(connfd);
						continue;
					}

					std::cout<<buf;
					write(connfd, buf, strlen(buf));
					
				}
		}
	}

	return 0;
}

