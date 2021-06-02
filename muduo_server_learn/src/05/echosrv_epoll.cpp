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
	//epoll_create1������Ϊ�ڲ�������һ�������ڴ棬������Ÿ���Ȥ��socket��һЩ�¼�
							//epoll_create(�ܹ������fd�ĸ��������������ϵͳ��Դ�ܹܶ���fd�������ж��٣����ܴ�����Ĳ���)
	epollfd = epoll_create1(EPOLL_CLOEXEC);//man epoll_create,epoll_create1=epoll_create+fcntl+EPOLL_CLOEXEC

	//�����൱�ڶԹ����ڴ���в��������ں�̬���в�����
	struct epoll_event event;
	event.data.fd = listenfd;
	event.events = EPOLLIN/* |Ĭ����LTģʽ�� EPOLLET*/;
	//epoll_ctl������ӵ������ڴ棬��û��epoll_create1��epoll_ctl�����൱�ڲ���һ������
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);//��listenfd��ӵ�epollfd���й�ע���Լ�����ע���¼���ӵ�epollfd���й���
	
	//�Ѹ���Ȥ���¼��ŵ���������
	//��ʼ״̬����Ҫ�й�ע���¼�
	EventList events(16);//��ʼ��һ���¼��б�
	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	int nready;
	while (1)
	{
		//&*events.begin()��һ��Ԫ�صĵ�ַ
		//static_cast<int>(events.size())��enents��С
		//-1����ʾ�ȴ�
		//���ص��¼�������&*events.begin(),������Ǹ��������������Ҫ���ݹ�ע���¼�����ע���¼���epoll_ctl�������ˣ���epollfd������
		//��poll��һ���������������Ҫ����������ݿ������ںˣ��Ͷ������ݿ���
		/*
		���������poll����Ӧ�ò������&*events.begin()����ģ��������ں�ȡ�����ˣ��ں˿�����һ�����ݽṹ������ע���¼��ŵ�������ݽṹ��
		epollfd = epoll_create1(EPOLL_CLOEXEC);
		������
		epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event)
		���ԣ�epoll��&*events.begin()ֻ��һ�������������ζ�ţ�ÿ�εȴ���ʱ�򣬲���Ҫÿ�ΰ�����Ҫ��ע���¼��������ںˣ�����Ҫÿ�δ��û��ռ佫���ݿ�����
		�ں˿ռ�
		*/
		//nready�����ص��¼�����
		//����poll�Ļ���������鿽�����ںˣ�����Ч�ʵ���
		//���ص��ǻ�Ծ��fd
		nready = epoll_wait(epollfd, &*events.begin(), static_cast<int>(events.size()), -1);//�൱��poll
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			
			ERR_EXIT("epoll_wait");
		}
		if (nready == 0)	// nothing happended
			continue;

		//���ص��¼�����=16��˵���ռ䲻���ˡ��ͳɱ�����
		if ((size_t)nready == events.size())
			events.resize(events.size()*2);

		for (int i = 0; i < nready; ++i)
		{
			//�����events���ǻ�Ծ��fd
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
				epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event);//��connfd�����עEPOLL_CTL_ADD
			}
			//������listenfd���ʹ���connfd
			else if (events[i].events & EPOLLIN)
			{
				connfd = events[i].data.fd;
				if (connfd < 0)
					continue;

				char buf[1024] = {0};
				int ret = read(connfd, buf, 1024);
				if (ret == -1)
					ERR_EXIT("read");
				if (ret == 0)//�Է��ر�
				{
					std::cout<<"client close"<<std::endl;
					close(connfd);
					event = events[i];
					epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, &event);//�޳�connfd���´β���ע
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
