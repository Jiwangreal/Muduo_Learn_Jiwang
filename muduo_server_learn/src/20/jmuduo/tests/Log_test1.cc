#include <muduo/base/Logging.h>
#include <errno.h>

using namespace muduo;

int main()
{
	//��ǰ����־������INFO�������2����־���������
	//���ص������������˲��������<<
	//LOG_TRACEʵ���ϵ������������󣬵�����stream()
	LOG_TRACE<<"trace ...";
	LOG_DEBUG<<"debug ...";


	LOG_INFO<<"info ...";
	LOG_WARN<<"warn ...";
	LOG_ERROR<<"error ...";//Ӧ�ü���Ĵ��󣬶���error����
	//LOG_FATAL<<"fatal ...";//��ѳ�����ֹabort()
	errno = 13;//ϵͳ����Ĵ���
	LOG_SYSERR<<"syserr ...";//����ݴ�������������־������error����
	LOG_SYSFATAL<<"sysfatal ...";//��ѳ�����ֹabort()
	return 0;
}
