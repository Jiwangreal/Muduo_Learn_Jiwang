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
	boost::shared_ptr<Y> p(new Y);//此时p的引用计数=1
	boost::shared_ptr<Y> q = p->f();//p->f()表示当前对象转换称为shared_ptr，此时引用计数=2

	Y* r = p->f2();
	assert(p == q);//断言其内部的引用计数是否相等
	assert(p.get() == r);//p.get()表示p里面的指针，其实就是new Y这个指针

	std::cout<<p.use_count()<<std::endl;

	// 这里构造了一个新的shared_ptr对象，并不是将一个shared_ptr对象赋值给另一个shared_ptr对象，后者的话引用计数会+1
	boost::shared_ptr<Y> s(r);//这里的r就是new Y这个指针，虽然2个指针的值是一样的，但是不代表shared_ptr引用计数+1，因为这里是构造了一个新的shared_ptr对象
						      //这里的r是裸指针，构造了一个新的shared_ptr对象
							//这里若想要引用计数值+1，必须是p对象拷贝构造s，或者将对象p赋值给s
	std::cout<<s.use_count()<<std::endl;
	assert(p == s);//这里断言会失败

	return 0;
}

