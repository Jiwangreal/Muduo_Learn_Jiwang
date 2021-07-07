#include "echo.h"

#include <muduo/base/Logging.h>

#include <boost/bind.hpp>

// using namespace muduo;
// using namespace muduo::net;

EchoServer::EchoServer(muduo::net::EventLoop* loop,
                       const muduo::net::InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer")
{
  // ���ӽ����������ӶϿ��Ļص��������󶨵�EchoServer::onConnection
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));

  //��Ϣ����Ļص��������󶨵� EchoServer::onMessage
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::start()
{
  server_.start();
}

// ��ص�Ӧ�ó�����߼���onConnection()��onMessage()��ʵ��
void EchoServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");//���ӽ�����ӡ���up�����ӶϿ���ӡ���down
}

void EchoServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
  muduo::string msg(buf->retrieveAllAsString());//buf->retrieveAllAsString()��ʾ�ӻ�������ȡ��
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
           << "data received at " << time.toString();
  conn->send(msg);
}

