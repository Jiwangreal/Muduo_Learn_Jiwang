
# if(BOOSTTEST_LIBRARY)表示如果安装了boost的测试库
if(BOOSTTEST_LIBRARY)
add_executable(inetaddress_unittest InetAddress_unittest.cc)
target_link_libraries(inetaddress_unittest muduo_net boost_unit_test_framework)
endif()
