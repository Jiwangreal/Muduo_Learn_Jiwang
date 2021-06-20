#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

//支持多线程对同一个文件写入
/*
多个线程对同一个文件进行写入，效率可能不如单个线程对同一个文件写入效率高
因为I/O总线并不一定能够并行，所以多个线程写入的时候会使用到异步日志。
异步日志：多个线程要写入日志的时候，会把数据发送到一个非线程安全的实际写入线程，让他排队写入，此时只有单个线程写入，不需要线程安全。
AsyncLogging实现了
*/
class LogFile : boost::noncopyable
{
 public:
  LogFile(const string& basename,
          size_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

  //将这一行logline的len长度的字符串添加进去
  void append(const char* logline, int len);
  void flush();//清空缓冲区

 private:
 //不加锁的方式添加，说明可以是线程安全的添加，多个线程可以用同一个对象添加文件
  void append_unlocked(const char* logline, int len);

  //获取日志文件名称
  static string getLogFileName(const string& basename, time_t* now);
  void rollFile();//回滚日志

  const string basename_;		// 日志文件basename
  const size_t rollSize_;		// 日志文件达到rolSize_换一个新文件
  const int flushInterval_;		// 日志写入间隔时间

  int count_;//计数器，count_>kCheckTimeRoll_时，会检测，是否达到日志的滚动条件，是否需要将日志写入到实际的文件中

  boost::scoped_ptr<MutexLock> mutex_;//MutexLock互斥量，scoped_ptr智能指针
  time_t startOfPeriod_;	// 开始记录日志时间（调整至零点的时间：距离1970年1月1日0点时间的秒数），只要不是新的一天的开始，这一天
                                //之内调整的零点时间距离1970年1月1日0点时间的秒数的都是一样的
                                //eg:当前时间是2021-06-07 21：48,会调整为2021-06-07 00：00，然后算它距离1970年1月1日0点时间的秒数
  time_t lastRoll_;			// 上一次滚动日志文件时间
  time_t lastFlush_;		// 上一次日志写入文件时间
  class File;//嵌套类，前向声明
  boost::scoped_ptr<File> file_;

  const static int kCheckTimeRoll_ = 1024;
  const static int kRollPerSeconds_ = 60*60*24;//1天
};

}
#endif  // MUDUO_BASE_LOGFILE_H
