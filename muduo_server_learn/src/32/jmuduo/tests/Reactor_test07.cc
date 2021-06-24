#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 用户回调函数
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

  InetAddress listenAddr(8888);//地址
  EventLoop loop;

  /*
  当客户端连接过来的时候，acceptor对象持有一个监听socket，它处于可读的状态；
  channel会观察监听socket的可读事件，channel就会处于活跃的通道，poller就会将其返回出来，调用handleEvent();
  handleEvent()又调用acceptor中的handleRead()，handleRead()又调用accept()来接受连接；
  接着会回调用户的newConnection这个函数；
  */ 
  Acceptor acceptor(&loop, listenAddr);//调用soclet的bind，listen
  acceptor.setNewConnectionCallback(newConnection);
  acceptor.listen();

  loop.loop();//事件循环
}

