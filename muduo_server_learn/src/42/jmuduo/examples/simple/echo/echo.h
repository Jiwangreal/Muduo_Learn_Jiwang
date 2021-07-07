#ifndef MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
#define MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H

#include <muduo/net/TcpServer.h>

// RFC 862
/*
ʹ��muduo��д���������������£�
�ṩһ��XXXXServer�ࡣ�ڸ����а���һ��TcpServer����

��Server��;�����TcpServer����Ĺ��ܣ� ������һ��tcp�������ˡ�����ע�������������¼����ɣ�
onConnection��
onMessage��
OnWriteComplete��

*/
class EchoServer
{
 public:
  EchoServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr);

  void start();  // calls server_.start();

 private:
//  EchoServer��ע���ӹرգ����ӶϿ����Լ���Ϣ����
// �����̹�ע���������¼������ӹرգ����ӶϿ�����Ϣ�����Ϣ�������(���ڵ������ķ�����˵��ͨ������Ҫ��ע���¼�)
  void onConnection(const muduo::net::TcpConnectionPtr& conn);//��ע�������ӽ��������ӶϿ����ص�onConnection()

  // ��ע������Ϣ����
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;//���ڶ�����˼��
};

#endif  // MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
