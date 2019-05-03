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
背景知识:

子进程完成任务之后一般不是由它自己回收掉自己，而是由父进程去回收子进程
在子进程完成任务之后 到 被父进程回收 的这段时间 这个子进程叫做 僵死进程

僵死进程存在的意义:
维护子进程的信息，方便父进程在以后某个时候获取,包括子进程的终止id,终止状态
以及资源利用信息等

但是僵死进程必须被回收！不然僵死进程的数量一多，
会占用大量的资源！，尤其是对我们这种长时间运行的服务器程序

进程终止---->进程僵死----->进程被回收

*/

//添加信号处理函数
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

void sig_chld(int signo) //回收僵死进程
{
	pid_t pid;
	int stat;

	/*
	waitpid函数:

    作用:回收进程

    参数:
    -1               :等待第一个终止的进程
    &stat            :获取终止进程的一些信息
    WNOHANG          :不阻塞(不管有没有子进程终止,立即返回)

    返回值:
    有子进程终止:     返回大于0的数
    没有子进程终止:   返回小于0的数
	*/
	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        printf("child %d terminated\n", pid);
    }

	return;
}

/*
  多进程+BIO 版本的web server!

  比单进程+BIO版本的server效率有所提高

  当没有连接到来的时候,该进程会阻塞在Accept函数上,
  当有多个连接到来的时候，为每一个连接fork一个进程

  当然,这里的connfd也是阻塞版本的！也就是说,在对connfd执行write,read等函数的时候也会阻塞.
  比如一个连接要先reda资源然后write资源，如果要reda的时候服务器没有准备好，那么系统会阻塞处理该连接的进程，等待要read的资源准备好

  因为是BIO的，每个进程可能因为连接没有请求到相应的资源而阻塞

  这样一来，单进程+BIO版本的server效率仍然不是很高！
*/
int main(int argc, char *argv[])
{
	int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

	while (true) /* 无限循环 */
	{
		struct sockaddr_in clientaddr;//客户端地址

		socklen_t len = sizeof(clientaddr);//客户端地址长度

		int connfd = Accept(listenfd, (SA*)&clientaddr, &len);//从阻塞队列中抽取一个连接,每次都是返回新的socket

		/*当一个子进程运行完毕之后，系统会给父进程发送一个SIGCHLD信号
		只有我们处理这个信号,就能够及时的回收子进程占用的资源*/
		addsig(SIGCHLD,sig_chld);//父进程处理SIGCHLD信号

		int pid;

		if((pid=Fork())==0)//当前进程为子进程
        {
            printf("\nnew process:%d\n", getpid());
            close(listenfd);//关闭子进程的端口监听

            doit(connfd);//子进程处理该连接

            close(connfd);//关闭子进程连接

	        printf("\nend of process:%d\n", getpid());
	        exit(0);
        }

        close(connfd);//关闭父进程连接
	}
	return 0;
}
