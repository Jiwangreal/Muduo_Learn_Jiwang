#include <muduo/base/FileUtil.h>

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

using namespace muduo;

int main()
{
  string result;
  int64_t size = 0;
  //��ȡ����������ļ���ʲô����
  int err = FileUtil::readFile("/proc/self", 1024, &result, &size);//size���ļ��Ĵ�С
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);//err�Ƿ��صĴ����룬/proc/self��һ��Ŀ¼�����Զ�ȡʧ��
  err = FileUtil::readFile("/proc/self", 1024, &result, NULL);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/proc/self/cmdline", 1024, &result, &size);///proc/self/cmdline������豸�ļ�
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/null", 1024, &result, &size);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 1024, &result, &size);//��/dev/zero��ȡ1024��0�ַ�
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  result.clear();
  err = FileUtil::readFile("/notexist", 1024, &result, &size);//�����ڵ��ļ�/notexist
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 102400, &result, &size);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
  err = FileUtil::readFile("/dev/zero", 102400, &result, NULL);
  printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
}

