// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

//�̰߳�ȫ����ģ��ʵ��
template<typename T>
class Singleton : boost::noncopyable
{
 public:
  //���ص�������
  static T& instance()
  {
    pthread_once(&ponce_, &Singleton::init);//pthread_once��֤init����ֻ������һ�Σ���֤����ʹ�����һ�Σ����ǵ�����
                                            //ͬ��Ҳ��֤���̰߳�ȫ����ȻҲ���������ķ�ʽʵ���̰߳�ȫ������Ч��û��pthread_once��
    return *value_;
  }

 private:
  Singleton();
  ~Singleton();

  //���ڲ���������
  static void init()
  {
    value_ = new T();
    ::atexit(destroy);//atexitע��һ�����������٣��Զ����٣����������ʱ���Զ�����һ�����������ٶ���
                      //ͬ��Ҳ����ʹ������ָ�룬��������ָ��������
  }

  //���ڲ����ٶ���
  static void destroy()
  {
    // T������һ����ȫ����
    /*
    incomplete_type���壺
    class A;ǰ�����������Ͳ���һ��complete_type
    ����Ȼ���Ե���ָ�룬Ҳ������deleteɾ����
    A* p;
    delete p;
    �����Ļ������벻����ֻ�ᱨ���档��������ڷ��գ���Ϊ����class A�ǲ�����������
    */
   //typedef������һ����������
   //char A[-1];����Ĵ�С������-1��ͨ���������ַ��ñ��뱨��
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//���ã��ڱ���׶η��ִ���
    delete value_;
  }

 private:
  static pthread_once_t ponce_;//�ö���֤��һ������ֻ��ִ��һ��
  static T*             value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}
#endif

