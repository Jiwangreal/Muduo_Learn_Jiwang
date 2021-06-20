#include <muduo/base/FileUtil.h>

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

using namespace muduo;

int main()
{
  string result;
  int64_t size = 0;
  //读取下面的特殊文件有什么特征
  int err = FileUtil::readFile("/proc/self", 1024, &result, &size);//size是文件的大小
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);//err是返回的错误码，/proc/self是一个目录，所以读取失败
  err = FileUtil::readFile("/proc/self", 1024, &result, NULL);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/proc/self/cmdline", 1024, &result, &size);///proc/self/cmdline特殊的设备文件
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/null", 1024, &result, &size);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 1024, &result, &size);//从/dev/zero读取1024个0字符
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  result.clear();
  err = FileUtil::readFile("/notexist", 1024, &result, &size);//不存在的文件/notexist
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 102400, &result, &size);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 102400, &result, NULL);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
}

