#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "csapp.h"
#include "epoll_ulti.h"
#include "http_server.h"

/* 网页的根目录 */
const char * root_dir = "/home/yb/WebSiteSrc/html_book_20150808/reference";
/* / 所指代的网页 */
const char * home_page = "index.html";

/*
* 多线程+BIO的web server.

  跟多进程+BIO的版本相差不大,都是为每个新的连接建立一个进程或者线程

  效率差不多,下一版本可以考虑采用线程池降低线程创建和销毁开销

*/

void* handle(void* arg)
{
	Pthread_detach(pthread_self());//与主线程分离,子线程结束后,资源自动回收

	int fd = (*(int *)arg);//本连接的套接字描述符

	Free(arg);  //释放为保证得到正确的fd值开辟的动态内存区,防止内存泄漏

	printf("%d: fd = %d\n", pthread_self(), fd);

	doit(fd);//处理该连接

	printf("%d: close fd = %d\n", pthread_self(), fd);

	close(fd);//关闭该连接,因为套接字描述符线程间是共享的,所以该套接字描述符只用关闭一次
	//父线程不用再关闭一次
}

int main(int argc, char *argv[])
{
	int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

	signal(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号  SIG_IGN代表忽略

	while (true) /* 无限循环 */
	{
		struct sockaddr_in clientaddr;
		pthread_t tid;//用于声明线程id

		socklen_t len = sizeof(clientaddr);

		int *fdp = (int *)Malloc(sizeof(int));//重新分配一块内存地址,保证线程安全

		//获得新的套接字描述符
		*fdp = Accept(listenfd, (SA*)&clientaddr, &len);//从阻塞队列抽取一个连接,每次返回的都是新的socket

		Pthread_create(&tid, NULL, handle, (void *)fdp);//为该连接建立一个线程,并设置线程运行函数和运行函数的参数

	}
	return 0;
}
/*

关于SIGPIPE信号的说明:

1.如果在写管道时读进程已终止，则产生此信号。

2.当类型为SOCK_STREAM的套接字已不再连接时，进程写到该套接字也产生此信号。

在服务器端场景下就是：
客户端程序向服务器端程序发送了消息，然后关闭客户端，
服务器端返回消息的时候就会收到内核给的SIGPIPE信号。

*/

/*
关于如何将新的套接字描述符传递给新线程:

最容易想到的方法:

connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
pthread_create(&th, NULL, thread_function, &connfd);

void *thread_function(void *arg) {
    int connfd = *((int*)arg);//赋值语句
    ...
}

这种做法会带来线程安全的问题,如果赋值语句在下一个Accept之前完成，那么线程中局部函数connfd将得到正确值

但是如果赋值语句在下一个Accept之后才完成，那么线程函数中局部变量cconnfd得到的将是下一个套接字描述符，这个显然不是我们想要的结果

正确的做法是每次调用Accept返回时,将返回的已连接套接字描述符存储在动态分配的内存中

这样无论线程中的赋值语句先于还是后于下一个Accept，都不会出现线程安全问题

在代码中的体现:int *fdp = (int *)Malloc(sizeof(int));//重新分配一块内存地址,保证线程安全

另外一个就是我们在线程运行函数中得到正确的connfd函数之后，需要将动态分配的内存释放！

在代码中的体现:

线程运行函数handle中:Free(arg);  //防止内存泄漏

*/

/*

关于 Pthread_create函数 的说明:

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp,void * (*routine)(void *), void *argp)

作用:创建线程

参数1:执行线程标识符的指针

参数2:设置线程属性

参数3:线程运行函数的地址

参数4:线程运行函数的参数

*/

/*
关于 Pthread_detach(pthread_self())语句是说明:

linux的线程状态和Windows不同

linux下线程有两种状态:joinable状态和unjoinable状态

pthread如果是joinable状态:当线程自己退出或pthread_exit时都不会释放线程占用的资源
只有调用phread_join之后这些资源才会被释放

pthread如果是unjoinable状态:当线程自己退出或pthread_exit时会自动释放线程占用的资源

所以！

调用Pthread_detach(pthread_self())就将线程的状态变为unjoinable，这样线程在自己结束或pthread_exit后就会自动释放资源
简单的说就是将子线程和主线程分离，这样子线程结束后会自动释放资源

关于pthread_join函数:

该函数的作用是子线程合并进入主线程，主线程阻塞等待子线程结束，然后回收子线程资源

该函数会使得阻塞，一般只有当子线程已经结束了的时候调用来回收子线程资源

子线程没有结束的话，不推荐使用，因为会阻塞主线程！

*/

