#include "epoll_ulti.h"

namespace utility
{
//日志打印函数
void mlog(pthread_t tid, const char *fileName, int lineNum, const char *func, const char *log_str, ...)
{
    va_list vArgList; //定义一个va_list型的变量,这个变量是指向参数的指针.
    char buf[1024];
    va_start(vArgList, log_str); //用va_start宏初始化变量,这个宏的第二个参数是第一个可变参数的前一个参数,是一个固定的参数
    vsnprintf(buf, 1024, log_str, vArgList); //注意,不要漏掉前面的_
    va_end(vArgList);  //用va_end宏结束可变参数的获取
    printf("%lu:%s:%d:%s --> %s\n", tid, fileName, lineNum, func, buf);
}

/* 将文件描述符设置为非阻塞 */
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);//获取文件标记

    //将其设置为非阻塞方式
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

/*增加一个fd*/
/*
one_shot:表示是否注册EPOLLONESHOT事件

注册成EPOLLONESHOT事件之后,那么操作系统一次做多只能触发fd上面的一个事件,也只能触发一个
避免了一个fd上某个事件被触发多次
要想重新触发，得重新注册事件

*/
void addfd(int epollfd, int fd, bool one_shot)
{
    //属性封装
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET; /* ET触发 */

    //根据参数注册EPOLLONESHOT事件
    if (one_shot)
    {
        event.events |= EPOLLONESHOT; /* 这里特别要注意一下 */
    }

    //开始添加fd
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    //设置成非阻塞形式
    setnonblocking(fd);
}

/*删除一个fd*/
void removefd(int epollfd, int fd)
{
    //mylog("removefd()\n");
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/*修改一个fd*/
void modfd(int epollfd, int fd, int ev, bool oneShot)
{
    //属性封装
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET; /* ET触发 */

    if (oneShot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//创建一个epoll句柄，该句柄会占用一个fd值，在使用完成后应该尽快关闭，避免fd不够用
int Epoll_create(int size)
{
    int rc;
    if ((rc = epoll_create(size)) < 0)
    {
        unix_error("epoll_crate failed");
    }
    return rc; /* 否则的话,返回文件描述符 */
}

//在指定时间内等待事件产生，得到事件集合
int Epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout)
{
    int rc;
    if ((rc = epoll_wait(epfd, events, maxevents, timeout)) == -1)
    {
        unix_error("epoll failed");
    }
    return rc;
}
}
