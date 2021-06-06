#include <muduo/base/Logging.h>

#include <muduo/base/CurrentThread.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Timestamp.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace muduo
{

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

__thread char t_errnobuf[512];
__thread char t_time[32];
__thread time_t t_lastSecond;

const char* strerror_tl(int savedErrno)
{
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

Logger::LogLevel initLogLevel()
{
	return Logger::TRACE;
	/*
  if (::getenv("MUDUO_LOG_TRACE"))
    return Logger::TRACE;
  else if (::getenv("MUDUO_LOG_DEBUG"))
    return Logger::DEBUG;
  else
    return Logger::INFO;
	*/
}

Logger::LogLevel g_logLevel = initLogLevel();

const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

// helper class for known string length at compile time
class T
{
 public:
  T(const char* str, unsigned len)
    :str_(str),
     len_(len)
  {
    assert(strlen(str) == len_);
  }

  const char* str_;
  const unsigned len_;
};

inline LogStream& operator<<(LogStream& s, T v)
{
  s.append(v.str_, v.len_);
  return s;
}

inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
{
  s.append(v.data_, v.size_);
  return s;
}

void defaultOutput(const char* msg, int len)
{
  size_t n = fwrite(msg, 1, len, stdout);
  //FIXME check n
  (void)n;
}

void defaultFlush()
{
  fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;//默认输出到标准输出
Logger::FlushFunc g_flush = defaultFlush;

}

using namespace muduo;

Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
  : time_(Timestamp::now()),//登记当前时间
    stream_(),
    level_(level),
    line_(line),
    basename_(file)
{
  formatTime();//格式化时间
  CurrentThread::tid();//缓存当前线程的ID
  stream_ << T(CurrentThread::tidString(), 6);//ID 所对应的字符串格式化到stream_
  stream_ << T(LogLevelName[level], 6);//格式化日志级别到stream_
  //savedErrno!=0,还需要输出errno所对对应的信息
  if (savedErrno != 0)
  {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

void Logger::Impl::formatTime()
{
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / 1000000);
  int microseconds = static_cast<int>(microSecondsSinceEpoch % 1000000);
  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
    ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime

    //格式化成%4d%02d%02d %02d:%02d:%02d格式
    int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    assert(len == 17); (void)len;
  }
  //格式化微妙
  Fmt us(".%06dZ ", microseconds);
  assert(us.length() == 9);
  
  //重载插入运算符，输出到LogStream所对应的缓冲区
  stream_ << T(t_time, 17) << T(us.data(), 9);
}

void Logger::Impl::finish()
{
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
  : impl_(INFO, 0, file, line)
{
}

//文件名，行，级别，函数名称
//0是里面的错误代码，表示没有错误
Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
  : impl_(level, 0, file, line)
{
  impl_.stream_ << func << ' ';//格式化函数名称
}

Logger::Logger(SourceFile file, int line, LogLevel level)
  : impl_(level, 0, file, line)
{
}

//日志级别：FATAL，ERROR
Logger::Logger(SourceFile file, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, file, line)
{
}

Logger::~Logger()
{
  impl_.finish();
  //注意这里buf是引用，直接引用了stream().buffer()缓冲区
  const LogStream::Buffer& buf(stream().buffer());
  g_output(buf.data(), buf.length());
  //FATAL会终止程序
  if (impl_.level_ == FATAL)
  {
    g_flush();
    abort();
  }
}

void Logger::setLogLevel(Logger::LogLevel level)
{
  g_logLevel = level;
}

void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}
