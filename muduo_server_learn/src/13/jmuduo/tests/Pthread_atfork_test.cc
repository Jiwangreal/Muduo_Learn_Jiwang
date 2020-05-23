#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

void prepare(void)
{
	printf("pid = %d prepare ...\n", static_cast<int>(getpid()));
}

void parent(void)
{
	printf("pid = %d parent ...\n", static_cast<int>(getpid()));
}

void child(void)
{
	printf("pid = %d child ...\n", static_cast<int>(getpid()));
}


int main(void)
{
	printf("pid = %d Entering main ...\n", static_cast<int>(getpid()));

/*
#include <pthread.h>
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
调用fork时，内部创建子进程前在父进程中会调用prepare，内部创建子进程成功后，父进程会调用parent ，子进程会调用child

*/
	pthread_atfork(prepare, parent, child);

	fork();//fork之后，父进程会调用parent，子进程会调用child，所以Exiting main会输出两次

	printf("pid = %d Exiting main ...\n",static_cast<int>(getpid()));

	return 0;
}
