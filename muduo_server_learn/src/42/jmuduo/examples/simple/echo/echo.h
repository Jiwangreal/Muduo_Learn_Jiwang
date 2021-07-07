#ifndef MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
#define MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H

#include <muduo/net/TcpServer.h>

// RFC 862
/*
使用muduo编写服务器，方法如下：
提供一个XXXXServer类。在该类中包含一个TcpServer对象，

该Server类就具有了TcpServer对象的功能， 他就是一个tcp服务器了。接着注册下面三个半事件即可：
onConnection；
onMessage；
OnWriteComplete；

*/
class EchoServer
{
 public:
  EchoServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr);

  void start();  // calls server_.start();

 private:
//  EchoServer关注连接关闭，连接断开，以及消息到达
// 网络编程关注的三个半事件：连接关闭，连接断开，消息到达，消息发送完毕(对于低流量的服务来说，通常不需要关注该事件)
  void onConnection(const muduo::net::TcpConnectionPtr& conn);//关注的是连接建立和连接断开，回调onConnection()

  // 关注的是消息到达
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;//基于对象编程思想
};

#endif  // MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
