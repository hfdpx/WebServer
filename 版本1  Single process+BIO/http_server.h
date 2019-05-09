#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_
#include "csapp.h"

/*
  请求处理函数

  fd  代表和客户端连接的socket描述符

*/
void doit(int fd);

/*
  请求头读入函数

  读入所有请求头信息

  fd  代表和客户端连接的socket描述符号

*/
void read_requesthdrs(rio_t *rp);

/*
  URL解析函数

  URL解析得到文件路径和CGI参数
  uri         URL连接
  filename    文件路径
  cgiargs     CGI参数
*/
int parse_uri(char *uri, char *filename, char *cgiargs);


/*
  静态网页处理函数

  fd         代表和客户端连接的socket描述符
  filename   文件所在路径
  filesize   文件大小

*/
void serve_static(int fd, char *filename, int filesize);

/*
  文件类型派生函数

  从文件名得到文件的类型
*/
void get_filetype(char *filename, char *filetype);

/*
  动态网页处理函数

  参数和静态处理函数的一样
*/
void serve_dynamic(int fd, char *filename, char *cgiargs);

/*
  错误处理函数
*/
void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg);

#endif
