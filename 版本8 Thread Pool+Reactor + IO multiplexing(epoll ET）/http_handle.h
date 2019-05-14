#ifndef _HTTP_HANDLE_H_
#define _HTTP_HANDLE_H_

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "csapp.h"
#include "noncopyable.h"
#include "state.h"
#include "cache.h"
#include "epoll_ulti.h"
#include "mutex.h"

/*
* HttpHandle类主要是用于处理http请求的,更具体来说,是为了处理静态网页.
*/

/* 这里默认是私有继承 */
class HttpHandle : noncopyable /* 不可以被拷贝,不可以被复制 */
{
public:
    //初始化
    void init(int fd);

    //连接处理函数
    void process();

    //epollfd的传递
    static void setEpollfd(int epollfd)
    {
        epollfd_ = epollfd;
    }
private:
    enum States {kRead, kWrite, kOver, kError};//状态

    //设置状态
    void setState(States s)
    {
        state_ = s;
    }
    //状态定义
    States state_;
private:
    //分析请求的 读操作
    int processRead();

    //响应请求 的写操作
    int processWrite();

    //重置
    void reset();

    //将回复的未知个数据参数添加到写缓冲区
    bool addResponse(const char* format, ...);

    //写入一个字符串进入 写缓冲区
    bool addResponse(char * const);

    //从读缓冲区中读取一行数据进入buf
    int getLine(char *buf, int maxsize);

    //通过文件后缀获得文件类型
    void static getFileType(char *fileName, char *fileType);

    //服务器错误处理函数
    void clienterror(char *cause, char *errnum, char *shortmsg, char *longmsg);

    //请求内容为静态内容处理函数
    void serveStatic(char *filename, int fileSize);

    //请求内容为动态内容处理函数
    void serveDynamic(char *filename, char *cgiargs);

    //读取请求头
    void readRequestHdrs();

    //解析URL 获得参数
    int parseUri(char *uri, char *fileName, char *cgiargs);

    //读取 读缓冲区中的数据
    bool read();

private:

    //读缓冲区的大小
    static const int READ_BUFFER_SIZE = 1024;

    //写缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;

    //网页的根目录
    static const char rootDir_[];

    //所指代的网页
    static const char homePage_[];

    //全局cache 只需要一个
    static Cache& cache_;

    //epoll描述符
    static int epollfd_;
private:

    //该连接的socket描述符
    int sockfd_;

    //文件
    boost::shared_ptr<FileInfo> fileInfo_;

    //读缓冲区
    char readBuf_[READ_BUFFER_SIZE];

    //标志读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    size_t nRead_;

    //当前正在分析的字符在读缓冲区中的位置
    size_t nChecked_;

    //是否保持连接
    bool keepAlive_;

    //是否发送文件
    bool sendFile_;

    //写缓冲区
    char writeBuf_[WRITE_BUFFER_SIZE];

    // 写缓冲区中待发送的字节数
    size_t nStored_;

    //已经写了多少字节
    size_t written_;
};
#endif
