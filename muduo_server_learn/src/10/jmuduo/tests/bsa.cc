#include <boost/static_assert.hpp>

class Timestamp
{
private:
	int64_t microSecondsSinceEpoch_;
};

//编译时的断言，来自boost库，assert是运行时的断言
BOOST_STATIC_ASSERT(sizeof(Timestamp) == sizeof(int64_t));//在编译的时候不会出错
//BOOST_STATIC_ASSERT(sizeof(int) == sizeof(short));//在编译的时候会出错

int main(void)
{
	return 0;
}
