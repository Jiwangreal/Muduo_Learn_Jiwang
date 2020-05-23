#include <stdio.h>

void foo(int x)
{
}

class B
{
public:
	virtual void foo()
	{
	}
};

class D : public B
{
public:
	void foo(int x)//派生类隐藏住了基类的虚函数，编译选项加上-Woverloaded-virtual就会显示这类警告
	{
	}
};

template<typename To, typename From>
inline To implicit_cast(From const &f) {
  return f;
}

int main(void)
{
	int n;
	double d = 1.23;
	n = d;

	B* pb;
	D* pd = NULL;

	pb = pd;

	//pd隐式转化成了pb，对比上面的写法而言，下面的更加清楚
	pb = implicit_cast<B*, D*>(pd);//这里测试 -Wconversion		// 一些可能改变值的隐式转换，给出警告，这里就不会给出警告
	return 0;
}
