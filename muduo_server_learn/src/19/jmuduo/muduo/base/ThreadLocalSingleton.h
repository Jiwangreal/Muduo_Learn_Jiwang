// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocalSingleton : boost::noncopyable
{
 public:

  //���ص������󣬷��ص��Ƕ��������
  static T& instance()
  {
    //����Ҫ�����̰߳�ȫ�ķ�ʽȥʵ�֣���Ϊÿ���̶߳���һ��ָ��
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);//��ָ�����ý���
    }
    return *t_value_;
  }

  //���ض����ָ��
  static T* pointer()
  {
    return t_value_;
  }

 private:

  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//����ȫ���ͣ��������ͻᱨ��
    delete t_value_;
    t_value_ = 0;
  }

  //�̱߳��ش洢2��TSD����ΪTSD�����õ�LinuxC��API�ӿ�ʵ�ֵģ���������ʹ����Ƕ����
  //Ƕ���࣬Ŀ�Ľ���ʹ�������ص�destructor����
  class Deleter
  {
   public:
    Deleter()
    {
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }

    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  //�̱߳��ش洢����1��__thread
  static __thread T* t_value_;//������ T*������__thread�ؼ��֣���ʾ��ָ�룬ÿһ���̶߳���һ��
  static Deleter deleter_;//����ָ����ָ��Ķ���Ŀ���ǵ���destructor()ʱ��ָ����ָ��Ķ����ܹ��Զ����ͷţ�����Ҫ��ʽ���ͷ�
                          //deleter_��������ʱ����>����~Deleter()��ͬʱҲ�����&ThreadLocalSingleton::destructor����>
                          //destructor()�����ĵ��û�����t_value_ָ��
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}
#endif
