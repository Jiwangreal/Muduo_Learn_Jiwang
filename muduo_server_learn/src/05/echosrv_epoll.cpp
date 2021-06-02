#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <vector>
#include <algorithm>
#include <iostream>

typedef std::vector<struct epoll_event> EventList;

#define ERR_EXIT(m) \
        do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	int listenfd;
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

	std::vector<int> clients;
	int epollfd;	
	//epoll_create1可以认为内部开辟了一个共享内存，用来存放感兴趣的socket的一些事件
							//epoll_create(能够处理的fd的个数，这个数随便填，系统资源能管多少fd，他就有多少，就能处理多大的并发)
	epollfd = epoll_create1(EPOLL_CLOEXEC);//man epoll_create,epoll_create1=epoll_create+fcntl+EPOLL_CLOEXEC

	//下面相当于对共享内存进行操作，在内核态进行操作的
	struct epoll_event event;
	event.data.fd = listenfd;
	event.events = EPOLLIN/* |默认是LT模式。 EPOLLET*/;
	//epoll_ctl把他添加到共享内存，若没有epoll_create1和epoll_ctl，则相当于操作一个数组
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);//将listenfd添加到epollfd进行关注，以及所关注的事件添加到epollfd进行管理
	
	//把感兴趣的事件放到数组里面
	//初始状态里面要有关注的事件
	EventList events(16);//初始化一个事件列表
	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	int nready;
	while (1)
	{
		//&*events.begin()第一个元素的地址
		//static_cast<int>(events.size())：enents大小
		//-1：表示等待
		//返回的事件都放在&*events.begin(),这仅仅是个输出参数，不需要传递关注的事件。关注的事件由epoll_ctl来传递了，由epollfd来管理，
		//而poll是一个输入输出参数，要把里面的数据拷贝到内核，就多了数据拷贝
		/*
		这块内容在poll中是应用层的数组&*events.begin()管理的，现在由内核取管理了，内核开辟了一个数据结构，将关注的事件放到这个数据结构中
		epollfd = epoll_create1(EPOLL_CLOEXEC);
		。。。
		epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event)
		所以，epoll的&*events.begin()只是一个输出参数，意味着：每次等待的时候，不需要每次把我们要关注的事件拷贝到内核，不需要每次从用户空间将数据拷贝到
		内核空间
		*/
		//nready：返回的事件个数
		//若是poll的话，会把数组拷贝到内核，所以效率低下
		//返回的是活跃的fd
		nready = epoll_wait(epollfd, &*events.begin(), static_cast<int>(events.size()), -1);//相当于poll
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			
			ERR_EXIT("epoll_wait");
		}
		if (nready == 0)	// nothing happended
			continue;

		//返回的事件个数=16，说明空间不够了。就成倍扩大
		if ((size_t)nready == events.size())
			events.resize(events.size()*2);

		for (int i = 0; i < nready; ++i)
		{
			//这里的events都是活跃的fd
			if (events[i].data.fd == listenfd)
			{
				peerlen = sizeof(peeraddr);
				connfd = ::accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

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


				std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<
					" port="<<ntohs(peeraddr.sin_port)<<std::endl;

				clients.push_back(connfd);
				
				event.data.fd = connfd;
				event.events = EPOLLIN/* | EPOLLET*/;
				epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event);//将connfd加入关注EPOLL_CTL_ADD
			}
			//处理完listenfd，就处理connfd
			else if (events[i].events & EPOLLIN)
			{
				connfd = events[i].data.fd;
				if (connfd < 0)
					continue;

				char buf[1024] = {0};
				int ret = read(connfd, buf, 1024);
				if (ret == -1)
					ERR_EXIT("read");
				if (ret == 0)//对方关闭
				{
					std::cout<<"client close"<<std::endl;
					close(connfd);
					event = events[i];
					epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, &event);//剔除connfd，下次不关注
					clients.erase(std::remove(clients.begin(), clients.end(), connfd), clients.end());
					continue;
				}

				std::cout<<buf;
				write(connfd, buf, strlen(buf));
			}

		}
	}

	return 0;
}
