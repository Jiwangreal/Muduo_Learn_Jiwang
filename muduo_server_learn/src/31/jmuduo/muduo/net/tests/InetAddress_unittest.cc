#include <muduo/net/InetAddress.h>

//#define BOOST_TEST_MODULE InetAddressTest
#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using muduo::string;
using muduo::net::InetAddress;

BOOST_AUTO_TEST_CASE(testInetAddress)
{
  // 构造一个地址，仅仅指定端口号是1234
  InetAddress addr1(1234);
  BOOST_CHECK_EQUAL(addr1.toIp(), string("0.0.0.0"));//ip应该等于0.0.0.0
  BOOST_CHECK_EQUAL(addr1.toIpPort(), string("0.0.0.0:1234"));
  // BOOST_CHECK_EQUAL(addr1.toHostPort(), string("0.0.0.0:1234"));

  InetAddress addr2("1.2.3.4", 8888);//ip+端口
  BOOST_CHECK_EQUAL(addr2.toIp(), string("1.2.3.4"));
  BOOST_CHECK_EQUAL(addr2.toIpPort(), string("1.2.3.4:8888"));

  // 255.255.255.255是广播地址
  InetAddress addr3("255.255.255.255", 65535);
  BOOST_CHECK_EQUAL(addr3.toIp(), string("255.255.255.255"));
  BOOST_CHECK_EQUAL(addr3.toIpPort(), string("255.255.255.255:65535"));
}
