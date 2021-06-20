#include "examples/simple/echo/echo.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <unistd.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  //�¼�ѭ������one loop per thread+threadpool
  //ÿ���߳����һ��EventLoop������û�оͲ���I/O�߳�
  muduo::net::EventLoop loop;//EventLoop���Ƕ�Reactorģʽ�ķ�װ
  muduo::net::InetAddress listenAddr(2007);
  //����multiple reactor�Ļ������reactor����mainrreactor���ڲ����ᴴ��subreactor
  EchoServer server(&loop, listenAddr);
  server.start();//�󶨼�����
  loop.loop();//��׽�¼���һ���¼������ص�EchoServer����Ӧ����
}

