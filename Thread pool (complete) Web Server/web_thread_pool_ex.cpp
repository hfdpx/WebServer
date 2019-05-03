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
#include "mutex.h"
#include "condition.h"

/*-
* 线程池的加强版本.主要是主线程统一接收连接,其余都是工作者线程,
  这里的布局非常类似于一个生产者，多个消费者.
*/


/* 网页的根目录 */
const char * root_dir = "/home/yb/WebSiteSrc/html_book_20150808/reference";
/* / 所指代的网页 */
const char * home_page = "index.html";

#define MAXNCLI 100//任务队列大小

MutexLock mutex;//锁
Condition cond(mutex);//为锁添加条件变量,实现同步

int clifd[MAXNCLI];//任务队列

int iput;//指向加入时间最短的任务(新加入的任务)
int iget;//指向加入时间最长的任务

int main(int argc, char *argv[])
{
    int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

    signal(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号

    pthread_t tids[10];//线程ID数组

    void* thread_main(void *);

    for (int i = 0; i < 10; ++i)
    {
        int *arg = (int *)Malloc(sizeof(int));//重新分配一块内存地址,保证线程安全
        *arg = i;

        Pthread_create(&tids[i], NULL, thread_main, (void *)arg);//建立一个线程,并设置线程运行函数和运行函数的参数
    }

    struct sockaddr cliaddr; /* 用于存储对方的ip信息 */

    socklen_t clilen;

    for (; ; )
    {
        int connfd = Accept(listenfd, &cliaddr, &clilen);//获取连接
        {
            MutexLockGuard lock(mutex); /* 给当前线程加锁 */

            clifd[iput] = connfd;//连接放入任务数组

            if (++iput == MAXNCLI) iput = 0;//任务数组满了，又从头开始放

            if (iput == iget)
                unix_error("clifd is not big enough!\n");
        }

        cond.notify(); /* 通知一个线程有数据啦! */
    }
    return 0;
}

//线程运行函数
void*
thread_main(void *arg)
{
    int connfd;
    printf("thread %d starting\n", *(int *)arg);
    Free(arg);//释放为保证得到正确的fd值开辟的动态内存区,防止内存泄漏

    for ( ; ;)
    {
        {
            MutexLockGuard lock(mutex); /* 给当前线程加锁 */
            while (iget == iput)   /* 任务队列中无连接 */
            {
                /*-
                * 使用while循环来等待条件变量的目的是解决虚假唤醒的问题
                */
                cond.wait(); /* 这一步会原子地unlock mutex并进入等待,wait执行完毕会自动重新加锁 */
            }

            connfd = clifd[iget]; /* 获得一个任务 */

            if (++iget == MAXNCLI) //任务数组尾部没有任务了，从头开始获取
                iget = 0;
        }
        doit(connfd);//处理连接
        close(connfd);//关闭连接
    }
}
