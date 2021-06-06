// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

//ThreadLocal��Ҳ�ǲ��ܿ�����
//�������ģ�巽ʽ��װ�� ����԰ѷ�POD���͵���Ž���
template<typename T>
class ThreadLocal : boost::noncopyable
{
 public:
  ThreadLocal()
  {
    //����key
    pthread_key_create(&pkey_, &ThreadLocal::destructor);//�ûص�����destructor������ʵ�ʵ�����
  }

  ~ThreadLocal()
  {
    //����key����������ʵ�����ݣ�
    pthread_key_delete(pkey_);
  }

  //ʵ�ʵ��ض�����
  T& value()
  {
    //��ȡ�߳��ض�����
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));//ת����T*
    //���ص�ָ�����ǿյ�
    if (!perThreadValue) 
    {
      T* newObj = new T();
      pthread_setspecific(pkey_, newObj);//�趨����
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    //�����һ����ȫ����
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete obj;
  }

 private:
  pthread_key_t pkey_;//key������
};

}
#endif
