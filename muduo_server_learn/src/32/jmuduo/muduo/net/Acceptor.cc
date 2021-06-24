// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie()),//����һ������socket
    acceptChannel_(loop, acceptSocket_.fd()),//��עacceptSocket_���¼�
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);//���õ�ַ�ظ�����
  acceptSocket_.bindAddress(listenAddr);//bind
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this));//acceptChannel_����һ��handleRead���Ļص�����
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();//�����е��¼�disable�������ܵ���remove()
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();//��עacceptChannel_�Ŀɶ��¼������ɶ��¼�������ʱ���ص���ǰbind��handleRead()����
}

void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(0);
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);//���Եȷ��ĵ�ַ���ص�peerAddr
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);//�ص�Ӧ�ò�Ļص�����
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of livev.
    // ����ʧ�ܣ�EMFILE��ʾ̫���fd
    if (errno == EMFILE)
    {
      // �ȹر�һ�����е�fd��Ŀ������accept()�ܹ����յ�
      // �������õ��ǵ�ƽ���������accept��һֱ���������Բ���������ƣ�
      // һaccept��fd���͹رյ���fd����Ϊfd̫���ˣ�����Ҫ׼��һ�����е�fd
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

