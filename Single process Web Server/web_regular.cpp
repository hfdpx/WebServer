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
/* 所指代的网页 */
const char * home_page = "index.html";
/*
* 单进程+BIO 版本的web server!

  当没有连接到来的时候,该进程会阻塞在Accept函数上,
  
  当有多个连接到来的时候，只能处理一个连接，其他连接进入阻塞队列等待，
  
  当然,这里的connfd也是阻塞版本的！也就是说,在对connfd执行write,read等函数的时候也会阻塞.

  比如一个连接要先reda资源然后write资源，如果要reda的时候服务器没有准备好，那么系统会阻塞处理该连接的进程，等待要read的资源准备好
  
  这样一来,这个版本的server效率会非常低下
*/
int main(int argc, char *argv[])
{
	int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

	while (true) /* 无限循环 */
	{
		struct sockaddr_in clientaddr;//客户端地址

		socklen_t len = sizeof(clientaddr);//客户端地址长度

		int connfd = Accept(listenfd, (SA*)&clientaddr, &len);//从阻塞队列中抽取一个连接

		doit(connfd);//处理该连接

		Close(connfd);//关闭该连接
	}
	return 0;
}
