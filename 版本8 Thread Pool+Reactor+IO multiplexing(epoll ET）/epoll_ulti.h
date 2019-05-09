#ifndef _EPOLL_ULTI_H_
#define _EPOLL_ULTI_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "csapp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define mylog(formatPM, args...)\
  mlog(pthread_self(), __FILE__, __LINE__, __FUNCTION__,  (formatPM) , ##args)

    //日志打印函数
	void mlog(pthread_t tid, const char *fileName, int lineNum, const char *func, const char *log_str, ...);

	//删除一个fd
	void removefd(int epollfd, int fd);

	//修改一个fd
	void modfd(int epollfd, int fd, int ev);

	//增加一个fd
	void addfd(int epollfd, int fd, bool one_shot);

	//在指定时间内等待事件产生，得到事件集合
	int Epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

	//创建一个epoll句柄
	int Epoll_create(int size);
#ifdef __cplusplus
}
#endif

#endif
