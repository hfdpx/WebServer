#include "http_server.h"


extern char* root_dir;
extern char* home_page;

/*请求处理函数*/
void doit(int fd)
{
    int is_static;         //请求的是否为静态文件
    struct stat sbuf;      //描述文件属性的结构,用来获取文件的信息
    char buf[MAXLINE];     //存放每次读取的一行数据
    char method[MAXLINE];  //存放请求方法 "GET"
    char uri[MAXLINE];     //存放URL链接 "http://www.baidu.com/
    char version[MAXLINE]; //存放http版本号 "HTTP/1.1\r\n"
    char filename[MAXLINE];//请求的文件路径
    char cgiargs[MAXLINE]; //CGI参数(动态网页具备的)
    rio_t rio;             //rio读缓冲区定义

    Rio_readinitb(&rio, fd);  //rio要先初始化

    Rio_readlineb(&rio, buf, MAXLINE);                      //读取一行数据进入buf中
    sscanf(buf, "%s %s %s", method, uri, version);          //从buf中读入请求方法,URL,HTTP版本号到对应字符数组

    if (strcasecmp(method, "GET"))                          //忽略大小比较字符串 服务器目前只支持GET方法
    {
        clienterror(fd, method, (char *)"501", (char *)"Not Implemented",(char *)"Tiny does not implement this method");//如果请求方法不是GET，返回501错误
        return;
    }

    read_requesthdrs(&rio);   //继续读入头部信息(仅读入,没有做其他处理)

    is_static = parse_uri(uri, filename, cgiargs);//解析URL得到请求的文件路径和相关参数

    if (stat(filename, &sbuf) < 0)    //将filename中信息存入sbuf中,执行成功返回0，失败返回-1,错误代码存于errno中
    {
        clienterror(fd, filename, (char *)"404", (char *)"Not found",(char *)"Tiny couldn't find this file"); //没有找到文件
        return;
    }

    /*请求内容为静态内容*/
    if (is_static)
    {

        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))   //该请求内容不为常规文件或者文件不可读
        {
            clienterror(fd, filename, (char *)"403", (char *)"Forbidden",(char *)"Tiny couldn't read the file"); //权限不够
            return;
        }
        /*静态网页处理函数*/
        serve_static(fd, filename, sbuf.st_size);
    }
    /*请求内容为动态内容*/
    else
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))   //该请求内容不为常规文件或者文件不可执行
        {
            clienterror(fd, filename, (char *)"403",(char *) "Forbidden",
                        (char *)"Tiny couldn't run the CGI program"); //服务器拒绝处理
            return;
        }
        /*动态网页处理函数*/
        serve_dynamic(fd, filename, cgiargs);
    }
}
/*
  读入请求头的所有信息

  其实只读了，这函数啥也没有干
*/
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);//从rp中读取一行数据进入buf数组

    while (strcmp(buf, "\r\n"))   //buf数组为"\r\n"则停止读入
    {

        Rio_readlineb(rp, buf, MAXLINE);//继续从rp中读入一行数据进入buf数组
        //printf("%s", buf);
    }
    return;
}

/*
  从URL中提取出文件路径,如果是动态网页的话,还需要提取出CGI参数信息
  并且返回值代表是否问静态网页

  返回值:
  如果是动态内容,返回0
  如果是静态内容,返回1

*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, (char *)"mwiki"))        //URL中不包含"mwiki"子串则代表请求的是静态内容
    {

        strcpy(cgiargs, "");                  //初始化成空
        strcpy(filename, root_dir);           //初始化为网页根目录
        strcat(filename, uri);                //URL拼接在网页根目录之后

        if (uri[strlen(uri) - 1] == '/')      //若 URL结尾字符为'/'
            strcat(filename, home_page);      //则将所指代的网页拼接到文件名之后 比如........\index.html

        //printf("filename: %s\n", filename);
        return 1;
    }
    else   //请求的是动态内容
    {

        ptr = index(uri, '?');//查找'?'首次出现的位置
        if (ptr)  //存在'?'字符
        {
            strcpy(cgiargs, ptr + 1);//'?'字符后面的就是CGI参数
            *ptr = '\0';// '\0' 代替 '?' 字符
        }
        else
            strcpy(cgiargs, "");   //URL中不存在'?'则CGI参数为空

        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

//静态网页处理函数
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp;
    char filetype[MAXLINE];   //文件类型
    char buf[MAXBUF];         //响应信息

    //得到文件类型
    get_filetype(filename, filetype);

    /*构造响应的头部*/
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

    /*发送头部*/
    Rio_writen(fd, buf, strlen(buf));

    /*发送主体*/
    srcfd = Open(filename, O_RDONLY, 0);  //以只读的方式打开文件
    srcp = (char *)Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//将文件映射进入内存
    Close(srcfd);                           //关闭文件
    Rio_writen(fd, srcp, filesize);         //发送文件
    Munmap(srcp, filesize);                 //解除内存映射
}

//从文件名得到文件类型
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg"); //image/png
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".ttf") || strstr(filename, ".otf"))
        strcpy(filetype, "application/octet-stream");
    else
        strcpy(filetype, "text/plain");
}

/*动态网页处理函数*/
void serve_dynamic(int fd, char *filename, char *cgiargs) /* 要获取动态的内容 */
{

    char buf[MAXLINE];
    char *emptylist[] = { NULL };

    /*构造和发送响应头部*/
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, (void *)"\r\n", 2);

    return;

    /*开个子进程为客户端运行CGI程序*/
    if (Fork() == 0)   //当前进程为子进程
    {

        /* 真实的服务器会在这里设置所有的cgi变量值 */
        setenv("QUERY_STRING", cgiargs, 1);    //根据CGI设置环境变量值
        Dup2(fd, STDOUT_FILENO);               //重定向到客户端
        Execve(filename, emptylist, environ);  //运行CGI程序
    }

    Wait(NULL);//父进程等待回收子进程资源 防止僵尸进程
}

/*错误处理函数*/
void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /*  建立 HTTP 响应主体 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* 发送 HTTP 响应 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

