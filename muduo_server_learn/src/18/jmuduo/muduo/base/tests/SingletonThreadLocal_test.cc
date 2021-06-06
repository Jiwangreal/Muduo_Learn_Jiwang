#include <muduo/base/Singleton.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/ThreadLocal.h>
#include <muduo/base/Thread.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <stdio.h>

class Test : boost::noncopyable
{
 public:
  Test()
  {
    printf("tid=%d, constructing %p\n", muduo::CurrentThread::tid(), this);
  }

  ~Test()
  {
    printf("tid=%d, destructing %p %s\n", muduo::CurrentThread::tid(), this, name_.c_str());
  }

  const std::string& name() const { return name_; }
  void setName(const std::string& n) { name_ = n; }

 private:
  std::string name_;
};

//��ʾmuduo::ThreadLocal<Test> ��������ǵ�����
//Test������ÿ�������еģ���Ϊ����TSD
//value()���߳��ض����ݣ�muduo::Singleton<muduo::ThreadLocal<Test> >::instance()�ǵ�������ÿ���̶߳����ʵ�ͬһ�����󣬿��Խ��俴����һ��ȫ�ֱ���
//muduo::Singleton<muduo::ThreadLocal<Test> >::instance().value()��ÿ���߳���ӵ�е�TSD������TSD����ÿ���̶߳���һ��
#define STL muduo::Singleton<muduo::ThreadLocal<Test> >::instance().value()

void print()
{
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),//�߳�id
         &STL,//��һ�Σ�һ������value()��������Test�Ĺ��캯������Test���ʹ��ݸ���ģ���T��
         STL.name().c_str());
}

void threadFunc(const char* changeTo)
{
  print();
  STL.setName(changeTo);
  sleep(1);
  print();
}

int main()
{
  //TSD����STL
  STL.setName("main one");
  
  //����2���߳�
  muduo::Thread t1(boost::bind(threadFunc, "thread1"));
  muduo::Thread t2(boost::bind(threadFunc, "thread2"));
  t1.start();
  t2.start();
  t1.join();
  print();
  t2.join();
  pthread_exit(0);
}
