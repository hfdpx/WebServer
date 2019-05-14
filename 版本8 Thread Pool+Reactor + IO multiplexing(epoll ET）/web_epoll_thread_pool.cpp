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
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "csapp.h"
#include "epoll_ulti.h"
#include "http_handle.h"
#include "thread_pool.h"

/*-
* 非阻塞版本的web server,主要利用epoll机制来实现多路IO复用.加上了线程池,这样以来可以实现更高的性能
*/

/*
原理: 监听套接字和连接加入epoll中，当监听套接字上有连接到来了，则将新连接加入epoll
      epoll中的连接可写或可读了则将其加入到任务队列，调用线程池的线程处理
*/

#define MAXEVENTNUM 100

void block_sigpipe()
{
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0)
    {
        printf("block sigpipe error\n");
    }
}

int main(int argc, char *argv[])
{
    int listenfd = Open_listenfd(8080); /* 8080号端口监听 */
    epoll_event events[MAXEVENTNUM];
    sockaddr clnaddr;
    socklen_t clnlen = sizeof(clnaddr);

    block_sigpipe(); /* 首先要将SIGPIPE消息阻塞掉 */

    //建立epoll
    int epollfd = Epoll_create(1024);

    //epollfd要监听listenfd上的可读事件
    addfd(epollfd, listenfd, false);

    //创建线程池 10线程,30000为任务队列长度最大数
    ThreadPool pools(10, 30000);

    //将epollfd转递进入HttpHandle
    HttpHandle::setEpollfd(epollfd);

    //处理http请求类
    HttpHandle handle[2000];

    for ( ; ;)
    {

        //到达的事件数量
        int eventnum = Epoll_wait(epollfd, events, MAXEVENTNUM, -1);

        for (int i = 0; i < eventnum; ++i)
        {
            int sockfd = events[i].data.fd;

            //有连接到来
            if (sockfd == listenfd)
            {
                //mylog("connection comes!");

                //处理所有到达的连接
                for ( ; ; )
                {
                    int connfd = accept(listenfd, &clnaddr, &clnlen);
                    if (connfd == -1)
                    {
                        //到达的连接处理完毕
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
                            break;
                        }
                        unix_error("accept error");
                    }

                    //将连接初始化
                    handle[connfd].init(connfd);

                    //将连接加入epoll监听，等待数据可读或者可写
                    addfd(epollfd, connfd, false);
                }
            }

            //有数据可读或可写,将其加入线程池的任务队列,等待线程处理
            else
            {
                //handle[sockfd]作为参数传入process函数
                pools.append(boost::bind(&HttpHandle::process, &handle[sockfd]));
            }
        }
    }
    return 0;
}

