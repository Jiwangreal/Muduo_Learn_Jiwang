#include "muduo/base/Timestamp.h"
#include <vector>
#include <stdio.h>

using muduo::Timestamp;

void passByConstReference(const Timestamp& x)
{
  printf("%s\n", x.toString().c_str());
}

void passByValue(Timestamp x)
{
  printf("%s\n", x.toString().c_str());
}

void benchmark()
{
  const int kNumber = 1000*1000;//const常量加个k，这是google的编码规范

  std::vector<Timestamp> stamps;
  stamps.reserve(kNumber);//预留了能容纳100万个Timestamp的空间
  for (int i = 0; i < kNumber; ++i)
  {
    //插入100万个时间
    stamps.push_back(Timestamp::now());//这里消耗的时间主要是now函数的gettimeofday,push_bak已经预留了空间，所以不会消耗时间
  }
  printf("%s\n", stamps.front().toString().c_str());
  printf("%s\n", stamps.back().toString().c_str());
  printf("%f\n", timeDifference(stamps.back(), stamps.front()));//计算最后一个时间和第一个时间的时间差

  int increments[100] = { 0 };
  int64_t start = stamps.front().microSecondsSinceEpoch();//相当于下标为0的时间
  for (int i = 1; i < kNumber; ++i)
  {
    int64_t next = stamps[i].microSecondsSinceEpoch();
    int64_t inc = next - start;//时间差
    start = next;
    if (inc < 0)
    {
      printf("reverse!\n");
    }
    else if (inc < 100)
    {
      ++increments[inc];//小于100的个数++
    }
    else
    {
      printf("big gap %d\n", static_cast<int>(inc));
    }
  }

  for (int i = 0; i < 100; ++i)
  {
    printf("%2d: %d\n", i, increments[i]);
  }
}

int main()
{
  //调用拷贝构造函数：用一个对象初始化另一个对象
  //Timestamp::now()是没有名称的对象，所以将其构造函数的值拷贝给now对象，看now里面的return代码就明白了！！
  Timestamp now(Timestamp::now());//等价于：Timestamp now=Timestamp::now()
  printf("%s\n", now.toString().c_str());
  passByValue(now);//值传递
  passByConstReference(now);//引用传递
  benchmark();//这是个基准函数
}

//看到了48：27