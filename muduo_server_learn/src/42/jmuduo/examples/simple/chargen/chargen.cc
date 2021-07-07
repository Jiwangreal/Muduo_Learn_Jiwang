#include "chargen.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

ChargenServer::ChargenServer(EventLoop* loop,
                             const InetAddress& listenAddr,
                             bool print)
  : loop_(loop),
    server_(loop, listenAddr, "ChargenServer"),
    transferred_(0),
    startTime_(Timestamp::now())
{
  server_.setConnectionCallback(
      boost::bind(&ChargenServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ChargenServer::onMessage, this, _1, _2, _3));

      // 关注消息发送完毕
      // onWriteComplete用于发送一组chargen字符串，再发送一组这样的字符串
  server_.setWriteCompleteCallback(
      boost::bind(&ChargenServer::onWriteComplete, this, _1));
  if (print)
  {
    // 每隔3秒，打印一下吞吐量
    loop->runEvery(3.0, boost::bind(&ChargenServer::printThroughput, this));
  }

  // 生成数据
  string line;
  for (int i = 33; i < 127; ++i)
  {
    line.push_back(char(i));
  }
  line += line;//33 34 35。。。。126

  // 生成94行
  /*
  33 34 35 。。。104
34 35 36 。。。105
。。。
55 56 57 。。。126
56 57 58 。。。126 33
。。。
126 33 34 35 .0。。 103
  */
  for (size_t i = 0; i < 127-33; ++i)
  {
    // line.substr(i, 72)从line里面取出72个字符
    message_ += line.substr(i, 72) + '\n';
  }
}

void ChargenServer::start()
{
  server_.start();
}

void ChargenServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "ChargenServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    conn->setTcpNoDelay(true);//开启nodelay，有数据则立刻发送，看一下服务器的吞吐量是多少
    conn->send(message_);
  }
}

void ChargenServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer* buf,
                              Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " discards " << msg.size()
           << " bytes received at " << time.toString();
}

// 一旦94行的数据发送完毕，继续发送下一个94行
void ChargenServer::onWriteComplete(const TcpConnectionPtr& conn)
{
  transferred_ += message_.size();//transferred_传输的字节数
  conn->send(message_);
}

void ChargenServer::printThroughput()
{
  Timestamp endTime = Timestamp::now();
  double time = timeDifference(endTime, startTime_);
  printf("%4.3f MiB/s\n", static_cast<double>(transferred_)/time/1024/1024);
  transferred_ = 0;
  startTime_ = endTime;
}

