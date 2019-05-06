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
#include "http_handle.h"
#include "state.h"

/*-
* 非阻塞版本的web server,主要利用epoll机制来实现多路IO复用.
* 将阻塞版本变为非阻塞版本之后,会增加复杂性,稍微举一个例子,因为文件描述符变得非阻塞了,往文件描述符里面写入数据的时候,
* 如果tcp缓存区已满,那么我们暂时还不能往里面写入数据,那么write函数会立马返回,而不是像以前那样阻塞在那里,一直等到缓冲区
* 可用.这样以来的话,由于一次性写不完,我们最好还要自己弄一个buf,还要记录已经写了多少数据,同时因为没有写完,所以还不能关闭
* 连接,要一直到写完才能关闭连接.这样以来程序的复杂度大大提高.
*/

/* 网页的根目录 */
const char * root_dir = "/home/yb/WebSiteSrc/html_book_20150808/reference";
/* / 所指代的网页 */
const char * home_page = "index.html";
#define MAXEVENTNUM 100

//信号处理
void addsig(int sig, void(handler)(int), bool restart = true)
{
	struct sigaction sa;

    /*
    struct sigaction
    {
       void (*sa_handler)(int);                           //信号处理函数
       void (*sa_sigaction)(int, siginfo_t *, void *);    //sigaction函数:用来查询或设置信号处理方式
       sigset_t sa_mask;                                  //用来设置在处理该信号时暂时将sa_mask 指定的信号集搁置
       int sa_flags;                                      //设置信号处理的相关操作  比如SA_RESTART:如果信号中断了某个进程的系统调用,则系统自动重启该进程的系统调用
       void (*sa_restorer)(void);                         //没有被使用
    };
    */
	memset(&sa, '\0', sizeof(sa));//初始化

	sa.sa_handler = handler;//设置信号处理函数

	if (restart)//根据需求设置sa_flags参数
	{
		sa.sa_flags |= SA_RESTART;
	}

	sigfillset(&sa.sa_mask);//清空信号集

	/*
	assert(false)  终止程序
	sigaction函数：查询和设置信号处理方式
	int sigaction(int signum, const struct sigaction *act,struct sigaction *oldact);
	参数:
	signum:  指出要捕获的信号类型
	act   :  指定新的信号处理方式
	oldact:  不为NULL则输出先前信号的处理方式
	返回值:0表示成功,-1表示有错误
	使用assert就是为了当sigaction函数产生错误的时候终止程序
	*/
	assert(sigaction(sig, &sa, NULL) != -1);
}


int main(int argc, char *argv[])
{
	int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

	epoll_event events[MAXEVENTNUM];//满足条件的fd数组

	sockaddr clnaddr;
	socklen_t clnlen = sizeof(clnaddr);

	//发送数据失败(client关闭了连接)的信号处理
	addsig(SIGPIPE, SIG_IGN);

	int epollfd = Epoll_create(80); //创建一个监听80个fd的epool

	addfd(epollfd, listenfd, false); // epollfd要监听listenfd上的可读事件

	HttpHandle handle[256];

	int acnt = 0;
	for ( ; ;) {

		int eventnum = Epoll_wait(epollfd, events, MAXEVENTNUM, -1);//获得可读 可写 事件

		for (int i = 0; i < eventnum; ++i) {

			int sockfd = events[i].data.fd;//获得该事件的文件描述符

            /* 有连接到来 */
			if (sockfd == listenfd) {

				printf("%d\n", ++acnt);

				int connfd = Accept(listenfd, &clnaddr, &clnlen);//获得连接的fd

				handle[connfd].init(connfd); /* 初始化 */

				addfd(epollfd, connfd, false); /* 加入监听 并且会将该连接设置成非阻塞形式 ,因为连接默认都是阻塞的 */

			}
			/* 有数据可读 */
			else if (events[i].events & EPOLLIN) {

				int res = handle[sockfd].processRead(); /* 处理读事件 */

				if (res == STATUS_WRITE)  /* 我们需要监听写事件 */
					modfd(epollfd, sockfd, EPOLLOUT);//修改标识符,下次写数据
				else
					removefd(epollfd, sockfd);//不需要监听写事件了则移除fd(LT模式,全部读完了)
			}
			/* 有数据可写 */
			else if (events[i].events & EPOLLOUT) {

				printf("Could write!\n");

				int res = handle[sockfd].processWrite(); /* 处理写事件 */

				if (res == STATUS_READ) /* 对方发送了keepalive */
					modfd(epollfd, sockfd, EPOLLIN);//修改标识符，下次读数据
				else
					removefd(epollfd, sockfd);//不需要保持长连接则可以移除fd了，因为LT模式,我都写完了
			}
		}
	}
	return 0;
}

/*
关于epoll的文件标识符:

EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；

EPOLLOUT：表示对应的文件描述符可以写；

EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；

EPOLLERR：表示对应的文件描述符发生错误；

EPOLLHUP：表示对应的文件描述符被挂断；

EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的；

EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里。

*/
