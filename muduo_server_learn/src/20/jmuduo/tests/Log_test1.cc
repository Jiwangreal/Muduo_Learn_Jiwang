#include <muduo/base/Logging.h>
#include <errno.h>

using namespace muduo;

int main()
{
	//当前的日志级别是INFO，下面的2个日志都不会输出
	//返回的是流，重载了插入运算符<<
	//LOG_TRACE实际上调用了匿名对象，调用了stream()
	LOG_TRACE<<"trace ...";
	LOG_DEBUG<<"debug ...";


	LOG_INFO<<"info ...";
	LOG_WARN<<"warn ...";
	LOG_ERROR<<"error ...";//应用级别的错误，都是error级别
	//LOG_FATAL<<"fatal ...";//会把程序终止abort()
	errno = 13;//系统级别的错误
	LOG_SYSERR<<"syserr ...";//会根据错误代码来输出日志，都是error级别
	LOG_SYSFATAL<<"sysfatal ...";//会把程序终止abort()
	return 0;
}
