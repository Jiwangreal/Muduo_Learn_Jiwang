#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

//֧�ֶ��̶߳�ͬһ���ļ�д��
/*
����̶߳�ͬһ���ļ�����д�룬Ч�ʿ��ܲ��絥���̶߳�ͬһ���ļ�д��Ч�ʸ�
��ΪI/O���߲���һ���ܹ����У����Զ���߳�д���ʱ���ʹ�õ��첽��־��
�첽��־������߳�Ҫд����־��ʱ�򣬻�����ݷ��͵�һ�����̰߳�ȫ��ʵ��д���̣߳������Ŷ�д�룬��ʱֻ�е����߳�д�룬����Ҫ�̰߳�ȫ��
AsyncLoggingʵ����
*/
class LogFile : boost::noncopyable
{
 public:
  LogFile(const string& basename,
          size_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

  //����һ��logline��len���ȵ��ַ�����ӽ�ȥ
  void append(const char* logline, int len);
  void flush();//��ջ�����

 private:
 //�������ķ�ʽ��ӣ�˵���������̰߳�ȫ����ӣ�����߳̿�����ͬһ����������ļ�
  void append_unlocked(const char* logline, int len);

  //��ȡ��־�ļ�����
  static string getLogFileName(const string& basename, time_t* now);
  void rollFile();//�ع���־

  const string basename_;		// ��־�ļ�basename
  const size_t rollSize_;		// ��־�ļ��ﵽrolSize_��һ�����ļ�
  const int flushInterval_;		// ��־д����ʱ��

  int count_;//��������count_>kCheckTimeRoll_ʱ�����⣬�Ƿ�ﵽ��־�Ĺ����������Ƿ���Ҫ����־д�뵽ʵ�ʵ��ļ���

  boost::scoped_ptr<MutexLock> mutex_;//MutexLock��������scoped_ptr����ָ��
  time_t startOfPeriod_;	// ��ʼ��¼��־ʱ�䣨����������ʱ�䣺����1970��1��1��0��ʱ�����������ֻҪ�����µ�һ��Ŀ�ʼ����һ��
                                //֮�ڵ��������ʱ�����1970��1��1��0��ʱ��������Ķ���һ����
                                //eg:��ǰʱ����2021-06-07 21��48,�����Ϊ2021-06-07 00��00��Ȼ����������1970��1��1��0��ʱ�������
  time_t lastRoll_;			// ��һ�ι�����־�ļ�ʱ��
  time_t lastFlush_;		// ��һ����־д���ļ�ʱ��
  class File;//Ƕ���࣬ǰ������
  boost::scoped_ptr<File> file_;

  const static int kCheckTimeRoll_ = 1024;
  const static int kRollPerSeconds_ = 60*60*24;//1��
};

}
#endif  // MUDUO_BASE_LOGFILE_H
