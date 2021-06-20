#include <muduo/base/Logging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/ThreadPool.h>

#include <stdio.h>

int g_total;
FILE* g_file;
boost::scoped_ptr<muduo::LogFile> g_logFile;

void dummyOutput(const char* msg, int len)
{
  g_total += len;
  if (g_file)
  {
    fwrite(msg, 1, len, g_file);
  }
  else if (g_logFile)
  {
    g_logFile->append(msg, len);
  }
}

void bench(const char* type)
{
  muduo::Logger::setOutput(dummyOutput);//默认输出，第一次什么都不做：既不输出到文件也不输出到标准输出
  muduo::Timestamp start(muduo::Timestamp::now());
  g_total = 0;

  int n = 1000*1000;
  const bool kLongLog = false;
  muduo::string empty = " ";
  muduo::string longStr(3000, 'X');//3000个X字符
  longStr += " ";
  for (int i = 0; i < n; ++i)
  {
    LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz"
             << (kLongLog ? longStr : empty)
             << i;
  }
  muduo::Timestamp end(muduo::Timestamp::now());
  double seconds = timeDifference(end, start);
  //打印到标准输出
  printf("%12s: %f seconds, %d bytes, %10.2f msg/s, %.2f MiB/s\n",
         type, seconds, g_total, n / seconds, g_total / seconds / (1024 * 1024));
}

void logInThread()
{
  //默认是标准输出，除非使用setOutput，子线程输出
  LOG_INFO << "logInThread";
  usleep(1000);
}

int main()
{
  getppid(); // for ltrace and strace，用于这俩命令

  muduo::ThreadPool pool("pool");
  pool.start(5);
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);

  //默认是标准输出，主线程食醋胡
  LOG_TRACE << "trace";
  LOG_DEBUG << "debug";
  LOG_INFO << "Hello";
  LOG_WARN << "World";
  LOG_ERROR << "Error";
  LOG_INFO << sizeof(muduo::Logger);
  LOG_INFO << sizeof(muduo::LogStream);
  LOG_INFO << sizeof(muduo::Fmt);
  LOG_INFO << sizeof(muduo::LogStream::Buffer);

  sleep(1);
  bench("nop");//性能测试：第一个是nop

  char buffer[64*1024];

  //测试文件写入到/dev/null的性能
  g_file = fopen("/dev/null", "w");
  setbuffer(g_file, buffer, sizeof buffer);
  bench("/dev/null");
  fclose(g_file);

  //测试文件写入到/tmp/log的性能
  g_file = fopen("/tmp/log", "w");
  setbuffer(g_file, buffer, sizeof buffer);
  bench("/tmp/log");
  fclose(g_file);

  //测试文件写入到test_log_st的性能，线程非安全写入
  g_file = NULL;
  g_logFile.reset(new muduo::LogFile("test_log_st", 500*1000*1000, false));
  bench("test_log_st");

  //测试文件写入到test_log_mt的性能，线程安全方式写入，效率可能比较上面的非安全的可能低一些
  g_logFile.reset(new muduo::LogFile("test_log_mt", 500*1000*1000, true));
  bench("test_log_mt");
  g_logFile.reset();
}
