add_executable(atomic_unittest Atomic_unittest.cc)
#target_link_libraries(atomic_unittest muduo_base)

add_executable(blockingqueue_test BlockingQueue_test.cc)
target_link_libraries(blockingqueue_test muduo_base)

add_executable(blockingqueue_bench BlockingQueue_bench.cc)
target_link_libraries(blockingqueue_bench muduo_base)

add_executable(boundedblockingqueue_test BoundedBlockingQueue_test.cc)
target_link_libraries(boundedblockingqueue_test muduo_base)

add_executable(exception_test Exception_test.cc)
target_link_libraries(exception_test muduo_base)

##新增
add_executable(logstream_bench LogStream_bench.cc)
target_link_libraries(logstream_bench muduo_base)

##新增，BOOSTTEST_LIBRARY看下boost测试框架库是否安装了
if(BOOSTTEST_LIBRARY)
add_executable(logstream_test LogStream_test.cc)
target_link_libraries(logstream_test muduo_base boost_unit_test_framework)
endif()

add_executable(mutex_test Mutex_test.cc)
target_link_libraries(mutex_test muduo_base)

add_executable(threadpool_test ThreadPool_test.cc)
target_link_libraries(threadpool_test muduo_base)

add_executable(singleton_test Singleton_test.cc)
target_link_libraries(singleton_test muduo_base)

add_executable(singletonthreadlocal_test SingletonThreadLocal_test.cc)
target_link_libraries(singletonthreadlocal_test muduo_base)

add_executable(thread_test Thread_test.cc)
target_link_libraries(thread_test muduo_base)

add_executable(threadlocal_test ThreadLocal_test.cc)
target_link_libraries(threadlocal_test muduo_base)

add_executable(threadlocalsingleton_test ThreadLocalSingleton_test.cc)
target_link_libraries(threadlocalsingleton_test muduo_base)

add_executable(timestamp_unittest Timestamp_unittest.cc)
target_link_libraries(timestamp_unittest muduo_base)

