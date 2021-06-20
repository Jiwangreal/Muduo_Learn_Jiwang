#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe，File类不是线程安全的
//File类
class LogFile::File : boost::noncopyable
{
 public:
 //构造函数
  explicit File(const string& filename)
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    assert(fp_);
    ::setbuffer(fp_, buffer_, sizeof buffer_);
    // posix_fadvise POSIX_FADV_DONTNEED ?
  }

  ~File()
  {
    ::fclose(fp_);
  }

  //len是总共要写的个数
  //n是已经写的个数
  void append(const char* logline, const size_t len)
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
	// remain>0表示没写完，需要继续写直到写完
    while (remain > 0)
    {
      size_t x = write(logline + n, remain);
      if (x == 0)
      {
        int err = ferror(fp_);
        if (err)
        {
          fprintf(stderr, "LogFile::File::append() failed %s\n", strerror_tl(err));
        }
        break;
      }
      n += x;
      remain = len - n; // 等价于remain -= x
    }

    writtenBytes_ += len;
  }

  void flush()
  {
    ::fflush(fp_);
  }

  size_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len)
  {
#undef fwrite_unlocked
    //这里用的是fwrite_unlocked，线程安全由LogFile类的mutex_保证
    return ::fwrite_unlocked(logline, 1, len, fp_);//不加锁的方式写入，效率高
                                                    //与fwrite区别仅在于是否加锁
  }

  FILE* fp_;
  char buffer_[64*1024];//文件指针的缓冲区
  size_t writtenBytes_;//已经写入的字节数
};

LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),//初始化给指针指针对象，所以就不需要做delete销毁操作了
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);//不包含/
  rollFile();//第一次则直接滚动一个文件
}

LogFile::~LogFile()
{
}

void LogFile::append(const char* logline, int len)
{
  if (mutex_)//线程啊暖，不为空
  {
    MutexLockGuard lock(*mutex_);//先加锁，再调用没有加锁的函数
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len)
{
  file_->append(logline, len);

  //写入文件后，需要判定是否需要滚动日志
  //writtenBytes()表示已经写入的字节数
  if (file_->writtenBytes() > rollSize_)
  {
    rollFile();
  }
  else
  {
    //先判断一个计数值
    if (count_ > kCheckTimeRoll_)
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_)//不等，说明是第二天的零点
      {
        rollFile();
      }
      else if (now - lastFlush_ > flushInterval_)
      {
        lastFlush_ = now;
        file_->flush();
      }
    }
    else
    {
      ++count_;
    }
  }
}

void LogFile::rollFile()
{
  time_t now = 0;
  string filename = getLogFileName(basename_, &now);//获取文件名称
  // 注意，这里先除kRollPerSeconds_ 后乘kRollPerSeconds_表示
  // 对齐至kRollPerSeconds_整数倍，也就是时间调整到当天零点。
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new File(filename));//产生一个新的日志文件
  }
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64);//文件名称的长度，eg：logfile_test.20130411-115604.popo.7743.log
  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: 也可以使用localtime_r ?，即GMT时间，gmtime_r与gmtime比较
                      //gmtime_r是线程安全的，即参数tm返回的就是线程安全的时间。gmtime只要函数返回值，其返回值不是线程安全的
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);//格式化时间
  filename += timebuf;
  filename += ProcessInfo::hostname();
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;
  filename += ".log";

  return filename;
}

