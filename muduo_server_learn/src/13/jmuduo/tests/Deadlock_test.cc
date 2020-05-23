// 一个在多线程程序里fork造成死锁的例子
// 一个输出示例：
/*

pid = 19445 Entering main ...
pid = 19445 begin doit ...
pid = 19447 begin doit ...
pid = 19445 end doit ...
pid = 19445 Exiting main ...

父进程在创建了一个线程，并对mutex加锁，
父进程创建一个子进程，在子进程中调用doit，由于子进程会复制父进程的内存，这时候mutex处于锁的状态，
父进程在复制子进程的时候，只会复制当前线程的执行状态，其它线程不会复制。因此子进程会处于死锁的状态。
*/
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* doit(void* arg)
{
	printf("pid = %d begin doit ...\n",static_cast<int>(getpid()));
	pthread_mutex_lock(&mutex);//已经是锁了，又调用了lock函数会造成死锁，2次加锁
	struct timespec ts = {2, 0};//等待2s
	nanosleep(&ts, NULL);
	pthread_mutex_unlock(&mutex);
	printf("pid = %d end doit ...\n",static_cast<int>(getpid()));

	return NULL;
}

int main(void)
{
	printf("pid = %d Entering main ...\n", static_cast<int>(getpid()));
	pthread_t tid;
	pthread_create(&tid, NULL, doit, NULL);//会先调用doit函数，然后睡眠2s
	struct timespec ts = {1, 0};//等待1s
	nanosleep(&ts, NULL);
	if (fork() == 0)//fork后，子进程会拷贝父进程所有的内存，mutex也会被拷贝一份，子进程拷贝下来就已经处于加锁状态
	{
		doit(NULL);//子进程运行到这里
	}
	pthread_join(tid, NULL);//父进程运行到这里
	printf("pid = %d Exiting main ...\n",static_cast<int>(getpid()));

	return 0;
}
