#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// �û��ص�����
void newConnection(int sockfd, const InetAddress& peerAddr)
{
  printf("newConnection(): accepted a new connection from %s\n",
         peerAddr.toIpPort().c_str());
  ::write(sockfd, "How are you?\n", 13);
  sockets::close(sockfd);
}

int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(8888);//��ַ
  EventLoop loop;

  /*
  ���ͻ������ӹ�����ʱ��acceptor�������һ������socket�������ڿɶ���״̬��
  channel��۲����socket�Ŀɶ��¼���channel�ͻᴦ�ڻ�Ծ��ͨ����poller�ͻὫ�䷵�س���������handleEvent();
  handleEvent()�ֵ���acceptor�е�handleRead()��handleRead()�ֵ���accept()���������ӣ�
  ���Ż�ص��û���newConnection���������
  */ 
  Acceptor acceptor(&loop, listenAddr);//����soclet��bind��listen
  acceptor.setNewConnectionCallback(newConnection);
  acceptor.listen();

  loop.loop();//�¼�ѭ��
}

