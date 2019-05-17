#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <functional>
#include "csapp.h"
#include "epoll_ulti.h"
#include "http_handle.h"
#include "thread_pool.h"

#include "time_heap.h"
#include "rlog.h"
using namespace utility;

/*-
* 非阻塞版本的web server,主要利用epoll机制来实现多路IO复用.加上了线程池,这样以来可以实现更高
* 的性能.
*/

#define MAXEVENTNUM 1024 //最大的事件数量
#define TIMESLOT 30  //连接超时时间
int epollfd;

void blockSig(int signo) /* 阻塞掉某个信号 */
{
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, signo);
    int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0)
    {
        unix_error("blockSig error");
    }
}
void cb_func(client_data *user_data)
{
   removefd(epollfd, user_data->sockfd);
}

int main(int argc, char *argv[])
{
    LOG_INIT("log","server log",2);//日志库初始化,2为默认日志级别(日志级别为1-2的日志才会被打印)

    int listenfd = Open_listenfd(8080);   /* 8080号端口监听 */

    epoll_event events[MAXEVENTNUM];      /* 事件集合 */

    sockaddr clnaddr;
    socklen_t clnlen = sizeof(clnaddr);

    blockSig(SIGPIPE);                    /* 首先要将SIGPIPE消息阻塞掉 */

    epollfd = Epoll_create(1024);     /* 创建epoll */
    addfd(epollfd, listenfd, false);      /* 将监听套接字放入epoll中监听连接的到来 */


    ThreadPool pools(10, 30000);          /*创建线程池*/
    HttpHandle::setEpollfd(epollfd);      /*设置事件的epollfd*/

    HttpHandle handle[2000];              /*事件设置*/

    for ( ; ;)
    {
        /*到达就绪的事件数量*/
        int eventnum = Epoll_wait(epollfd, events, MAXEVENTNUM, -1);

        for (int i = 0; i < eventnum; ++i)
        {
            /* 获得事件的文件描述符 */
            int sockfd = events[i].data.fd;

            /* 该事件为有新连接到达 */
            if (sockfd == listenfd)
            {
                /* 因为ET模式 所以到达的连接一次性要处理完 */
                for ( ; ; )
                {
                    /* 获得连接的文件描述符 */
                    int connfd = accept(listenfd, &clnaddr, &clnlen);

                    DEBUG("The new connection [%d] is coming !",connfd);//log

                    if (connfd == -1)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))   /* 将连接已经建立完了 */
                        {
                            break;
                        }
                        unix_error("accept error");
                    }

                    //定时器初始化并且将定时器和连接捆绑
                    client_data *users =new client_data[1];
                    users[0].sockfd=connfd;
                    heap_timer *timer = new heap_timer(TIMESLOT);
                    timer->user_data=&users[0];
                    timer->cb_func=cb_func;

                    //在连接初始化的时候并没有将连接的定时器加入到小根堆，因为我们处理连接的速度是很快的，没有必要连接刚刚到就给它加入小根堆，这样可以避免小根堆元素的膨胀
                    //只有实在经过了一次处理之后还没有被关闭的连接才加入小根堆定时器
                    handle[connfd].init(connfd,timer);   /*  连接初始化      */
                    addfd(epollfd, connfd, true);  /*  将连接加入监听 */

                    DEBUG("The new connection [%d] has been initialized and monitored!",connfd);
                }
            }
            /* 该事件为连接上有数据可读或者可写 */
            else
            {
                DEBUG("Date is readable or writable on the connection [%d]",sockfd);
                pools.append(std::bind(&HttpHandle::process, &handle[sockfd])); /* 多线程版本 */
            }
        }
    }
    return 0;
}

