#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>
#include <sys/resource.h>

// 滚动大小是500M
int kRollSize = 500*1000*1000;

muduo::AsyncLogging* g_asyncLog = NULL;

void asyncOutput(const char* msg, int len)
{
  g_asyncLog->append(msg, len);
}

void bench(bool longLog)
{
  // 前端业务写线程
  muduo::Logger::setOutput(asyncOutput);

  int cnt = 0;
  const int kBatch = 1000;
  muduo::string empty = " ";
  muduo::string longStr(3000, 'X');//3000个X字符
  longStr += " ";

  // 30000次循环
  for (int t = 0; t < 30; ++t)
  {
    muduo::Timestamp start = muduo::Timestamp::now();//当前时间
    for (int i = 0; i < kBatch; ++i)
    {
      // LOG_INFO写入日志时，实际上是通过asyncOutput来写入日志，相当于前端线程往异步日志中append添加消息到缓冲区中
      LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz "
               << (longLog ? longStr : empty)
               << cnt;
      ++cnt;
    }
    muduo::Timestamp end = muduo::Timestamp::now();
    printf("%f\n", timeDifference(end, start)*1000000/kBatch);//timeDifference(end, start)单位是秒，timeDifference(end, start)*1000000/kBatch=毫秒
	// 注释掉，加剧消息堆积
    struct timespec ts = { 0, 500*1000*1000 };//睡眠0.5秒
    nanosleep(&ts, NULL);
  }
}

int main(int argc, char* argv[])
{
  {
    // 设置虚拟内存ulimit -a ,因为mmap分配内存的时候不能多于2G，这里设置成2G之后，就不能分配大于2G的内存，mallocc
    // 内部其实调用blk来分配内存
    // set max virtual memory to 2GB.
    size_t kOneGB = 1000*1024*1024;
    rlimit rl = { 2*kOneGB, 2*kOneGB };
    setrlimit(RLIMIT_AS, &rl);
  }

  printf("pid = %d\n", getpid());

  char name[256];
  strncpy(name, argv[0], 256);
  
  //构造一个异步日志类
  muduo::AsyncLogging log(::basename(name), kRollSize);
  log.start();//启动
  g_asyncLog = &log;

  bool longLog = argc > 1;
  bench(longLog);
}
