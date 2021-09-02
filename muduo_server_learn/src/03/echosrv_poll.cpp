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

//��������̬���飩�������еĿռ��������ģ����Ե�����������
typedef std::vector<struct pollfd> PollFdList;

int main(void)
{
	//SIGPIPE������
	//��Linux�������У�12����11��״̬
	//����ͻ��˹ر��׽���close��������˵�����һ��write�������������һ��RST segment(��TCO�����)
	//���������ٴε�����write�����ʱ��ͻ����SIGPIPE�ź�
	//���û�к��Ը��źŵĻ������źŵ�Ĭ�ϴ���ʽ�����˳���������
	//�߿��õķ�������Ҫ7*24����������Ҫ���Ե���

	//TIME_WAIT״̬�Դ󲢷���������Ӱ��
	//Ӧ�������ڷ������˱������TIME_WAIT״̬
	//����������������Ͽ����ӣ�����client����close��������˾ͻ����TIME_WAIT�������Ļ������ں��У�����һ��ʱ����holdס�ں���Դ��
	//ʹ�ò���������󽵵��ˣ�����Ҫ����time_wait״̬
	//��δ���TIME_WAIT״̬��
	//Э������ϣ�Ӧ���ÿͻ��������Ͽ����ӣ������Ͱ�TIME_WAIT״̬��ɢ�������Ŀͻ��ˡ���������ͻ��˲���Ծ�ˣ�
	//һЩ�ͻ��˲��Ͽ����ӣ������ͻ�ռ�÷����������ӵ���Դ�����⣬�����ҲҪ�и��������ߵ�����Ծ�����ӣ�������close������û�£�
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);//���⽩������

	//int idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	int listenfd;

	//nonblocking socket +IO��·����
	//SOCK_NONBLOCK��������״̬
	//SOCK_CLOEXEC�����̱��滻ʱ��fd���ڹر�״̬��eg��forkһ�������ְ����滻������ʱfd�ǹرյ�
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
	pfd.events = POLLIN;//��ע����POLLIN�¼��������ݿɶ���man poll

	PollFdList pollfds;
	pollfds.push_back(pfd);//����listenfd�Ŀɶ��¼�

	int nready;

	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int connfd;

	while (1)
	{
		//pollfds.begin()����̬������׵�ַ����һ������ֵ��*pollfds.begin()
		//��һ������ֵ���׵�ַ��&*pollfds.begin()����C++11�У�����д�ɣ�pollfds.data()
		//pollfds.size()������ĸ�����fd�ĸ�����vector�����Ĵ�С��
		//-1�������Ƶȴ�
		nready = poll(&*pollfds.begin(), pollfds.size(), -1);

		//������ڶ�����continue��ϱ��棺pollfds��������2��fd��listenfd��connfd
		//��������listenfd���ܲ����˿ɶ��¼���connfdҲ���ܲ����˿ɶ��¼�
		if (nready == -1)
		{
			if (errno == EINTR)
				continue;
			
			ERR_EXIT("poll");
		}
		if (nready == 0)	// nothing happended
			continue;
		
		//pollfds�����е�һ��fd���Ǽ���fd
		if (pollfds[0].revents & POLLIN)
		{
			peerlen = sizeof(peeraddr);
			/*
			C++��ʾʹ��ȫ�ֵĺ���: ::accept4��û��ͬ���ģ�������ν��::
			connfd = ::accept4(listenfd, (struct sockaddr*)&peeraddr,
						&peerlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
			*/
			//accept����û�У�SOCK_NONBLOCK | SOCK_CLOEXEC��flag
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
			pfd.revents = 0;//��ǰֻ�Ǽ���listen��û���κ��¼�����
			pollfds.push_back(pfd);
			--nready;

			// ���ӳɹ�
			std::cout<<"ip="<<inet_ntoa(peeraddr.sin_addr)<<
				" port="<<ntohs(peeraddr.sin_port)<<std::endl;
			if (nready == 0)//˵���¼�����������
				continue;
				
		}

		//std::cout<<pollfds.size()<<std::endl;
		//std::cout<<nready<<std::endl;

		//�����ļ���connfd�����˿ɶ��¼�
		//pollfds.begin()+1��ʼ�ǣ�������fd
		for (PollFdList::iterator it=pollfds.begin()+1;
			it != pollfds.end() && nready >0; ++it)
		{
				if (it->revents & POLLIN)//���ص��¼����пɶ��¼�
				{
					--nready;
					connfd = it->fd;//ȡ��������fd
					char buf[1024] = {0};
					int ret = read(connfd, buf, 1024);
					if (ret == -1)
						ERR_EXIT("read");
					if (ret == 0)//����0��˵���Է��ر���socket
					{
						std::cout<<"client close"<<std::endl;
						//��������fd������vector���Ƴ�
						it = pollfds.erase(it);//Ϊʲô������vector���������飿
						--it;//ΪʲôҪ--it��

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

