#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe��File�಻���̰߳�ȫ��
//File��
class LogFile::File : boost::noncopyable
{
 public:
 //���캯��
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

  //len���ܹ�Ҫд�ĸ���
  //n���Ѿ�д�ĸ���
  void append(const char* logline, const size_t len)
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
	// remain>0��ʾûд�꣬��Ҫ����дֱ��д��
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
      remain = len - n; // �ȼ���remain -= x
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
    //�����õ���fwrite_unlocked���̰߳�ȫ��LogFile���mutex_��֤
    return ::fwrite_unlocked(logline, 1, len, fp_);//�������ķ�ʽд�룬Ч�ʸ�
                                                    //��fwrite����������Ƿ����
  }

  FILE* fp_;
  char buffer_[64*1024];//�ļ�ָ��Ļ�����
  size_t writtenBytes_;//�Ѿ�д����ֽ���
};

LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),//��ʼ����ָ��ָ��������ԾͲ���Ҫ��delete���ٲ�����
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);//������/
  rollFile();//��һ����ֱ�ӹ���һ���ļ�
}

LogFile::~LogFile()
{
}

void LogFile::append(const char* logline, int len)
{
  if (mutex_)//�̰߳�ů����Ϊ��
  {
    MutexLockGuard lock(*mutex_);//�ȼ������ٵ���û�м����ĺ���
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

  //д���ļ�����Ҫ�ж��Ƿ���Ҫ������־
  //writtenBytes()��ʾ�Ѿ�д����ֽ���
  if (file_->writtenBytes() > rollSize_)
  {
    rollFile();
  }
  else
  {
    //���ж�һ������ֵ
    if (count_ > kCheckTimeRoll_)
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_)//���ȣ�˵���ǵڶ�������
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
  string filename = getLogFileName(basename_, &now);//��ȡ�ļ�����
  // ע�⣬�����ȳ�kRollPerSeconds_ ���kRollPerSeconds_��ʾ
  // ������kRollPerSeconds_��������Ҳ����ʱ�������������㡣
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new File(filename));//����һ���µ���־�ļ�
  }
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64);//�ļ����Ƶĳ��ȣ�eg��logfile_test.20130411-115604.popo.7743.log
  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: Ҳ����ʹ��localtime_r ?����GMTʱ�䣬gmtime_r��gmtime�Ƚ�
                      //gmtime_r���̰߳�ȫ�ģ�������tm���صľ����̰߳�ȫ��ʱ�䡣gmtimeֻҪ��������ֵ���䷵��ֵ�����̰߳�ȫ��
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);//��ʽ��ʱ��
  filename += timebuf;
  filename += ProcessInfo::hostname();
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;
  filename += ".log";

  return filename;
}

