#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <cassert>

class Y: public boost::enable_shared_from_this<Y>
{
public:
	boost::shared_ptr<Y> f()
	{
		return shared_from_this();
	}

	Y* f2()
	{
		return this;
	}
};

int main()
{
	boost::shared_ptr<Y> p(new Y);//��ʱp�����ü���=1
	boost::shared_ptr<Y> q = p->f();//p->f()��ʾ��ǰ����ת����Ϊshared_ptr����ʱ���ü���=2

	Y* r = p->f2();
	assert(p == q);//�������ڲ������ü����Ƿ����
	assert(p.get() == r);//p.get()��ʾp�����ָ�룬��ʵ����new Y���ָ��

	std::cout<<p.use_count()<<std::endl;

	// ���ﹹ����һ���µ�shared_ptr���󣬲����ǽ�һ��shared_ptr����ֵ����һ��shared_ptr���󣬺��ߵĻ����ü�����+1
	boost::shared_ptr<Y> s(r);//�����r����new Y���ָ�룬��Ȼ2��ָ���ֵ��һ���ģ����ǲ�����shared_ptr���ü���+1����Ϊ�����ǹ�����һ���µ�shared_ptr����
						      //�����r����ָ�룬������һ���µ�shared_ptr����
							//��������Ҫ���ü���ֵ+1��������p���󿽱�����s�����߽�����p��ֵ��s
	std::cout<<s.use_count()<<std::endl;
	assert(p == s);//������Ի�ʧ��

	return 0;
}

