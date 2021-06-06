#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Thread.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <string>
#include <stdio.h>

using namespace muduo;

class Test
{
 public:
  Test(int numThreads)
    : latch_(1),
      threads_(numThreads)
  {
    //����3���߳�
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      
      //�̵߳Ļص�������threadFunc
      //�̵߳����ƣ�name
      threads_.push_back(new muduo::Thread(
            boost::bind(&Test::threadFunc, this), muduo::string(name)));
    }
    //��begin��endִ�У�boost::bind(&Thread::start, _1)����_1���ݵĲ���������start
    //���ص���boost::function
    /*
    boost::bind(&Thread::start, _1)���ú����������
    ������������()�����
    class foo
    {
      operator()(Thread* t)
    };
    */
    for_each(threads_.begin(), threads_.end(), boost::bind(&Thread::start, _1));
  }

  void run()
  {
    latch_.countDown();//����������Ϊ0
  }

  void joinAll()
  {
    for_each(threads_.begin(), threads_.end(), boost::bind(&Thread::join, _1));
  }

 private:

  //�����̶߳������ˣ�����threadFunc
  void threadFunc()
  {
    latch_.wait();//�����̶߳��ڵȴ����̷߳���ʩ��ȴ�������=0
    printf("tid=%d, %s started\n",
           CurrentThread::tid(),
           CurrentThread::name());

    

    printf("tid=%d, %s stopped\n",
           CurrentThread::tid(),
           CurrentThread::name());
  }

  CountDownLatch latch_;
  boost::ptr_vector<Thread> threads_;
};

int main()
{
  //���̵߳�PID��TID
  printf("pid=%d, tid=%d\n", ::getpid(), CurrentThread::tid());
  Test t(3);
  sleep(3);//sllep 3s�����߳��ǲ����е�
  printf("pid=%d, tid=%d %s running ...\n", ::getpid(), CurrentThread::tid(), CurrentThread::name());
  t.run();//���̷߳���ʦ��
  t.joinAll();

  printf("number of created threads %d\n", Thread::numCreated());
}


