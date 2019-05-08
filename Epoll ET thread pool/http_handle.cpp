#include "http_handle.h"

Cache g_cache;

Cache& HttpHandle::cache_ = g_cache;

int HttpHandle::epollfd_ = -1;

const char HttpHandle::rootDir_[] = "/home/yb/WebSiteSrc/html_book_20150808/reference";
const char HttpHandle::homePage_[] = "index.html";

/*通过文件后缀获得文件的类型*/
void HttpHandle::getFileType(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".ttf") || strstr(filename, ".otf"))
        strcpy(filetype, "application/octet-stream");
    else
        strcpy(filetype, "text/plain");
}

/* 要获取的内容为静态内容 */
void HttpHandle::serveStatic(char *fileName, int fileSize)
{
    int srcfd;
    char fileType[MAXLINE], buf[MAXBUF];

    //构造头部信息
    getFileType(fileName, fileType);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, fileSize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, fileType);

    //将数据写入 写缓冲区
    addResponse(buf);

    //发生主体
    //主体为已经写好的文件
    cache_.getFileAddr(fileName, fileSize, fileInfo_); /* 添加文件 */ //文件是映射进自己定义的Cache中的   //!!!亮点

    //打印文件被使用次数
    mylog("fileInfo_ use count: %d", fileInfo_.use_count());


    //更改标识,准备向客户发生数据
    sendFile_ = true;
}

/*错误处理函数*/
void HttpHandle::clienterror(char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* 构造 HTTP 响应主体 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* 发送 HTTP 响应 */
    addResponse("HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    addResponse("Content-type: text/html\r\n");
    addResponse("Content-length: %d\r\n\r\n", (int)strlen(body));
    addResponse("%s", body);
}

/* 读取请求头 */
void HttpHandle::readRequestHdrs()
{
    char buf[MAXLINE];
    getLine(buf, MAXLINE);//读入一行

    //打印
    mylog("buf: %s", buf);
    mylog("readBuf: %s", readBuf_);
    mylog("read:%d", nRead_);

    while (strcmp(buf, "\r\n"))
    {

        getLine(buf, MAXLINE);

        //打印从读缓冲区中读到的数据
        mylog("buf: %s", buf);

        //根据参数设置是否需要长连接
        if (strstr(buf, "keep-alive"))
        {
            keepAlive_ = true; /* 保持连接 */
        }
    }
    return;
}

//从URL中提取出文件路径,如果是动态网页的话,还需要提取出CGI参数信息，并且返回值代表是否问静态网页
int HttpHandle::parseUri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, (char *)"mwiki"))        //URL中不包含"mwiki"子串则代表请求的是静态内容
    {

        strcpy(cgiargs, "");                  //初始化成空
        strcpy(filename,rootDir_);           //初始化为网页根目录
        strcat(filename, uri);                //URL拼接在网页根目录之后

        if (uri[strlen(uri) - 1] == '/')      //若 URL结尾字符为'/'
            strcat(filename,homePage_);      //则将所指代的网页拼接到文件名之后 比如........\index.html

        //打印客户要获取的文件名
        mylog("filename:%s\n",filename);
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


/*从读缓冲区中读取一行数据进入buf中*/
int HttpHandle::getLine(char *buf, int maxsize)
{
    int n; /* 用于记录读取的字节的数目 */

    for (n = 0; nChecked_ < nRead_;)
    {

        *buf++ = readBuf_[nChecked_];
        n++;

        if (readBuf_[nChecked_++] == '\n')
        {
            break;
        }

    }
    *buf = 0;
    return n;
}


int HttpHandle::processRead()
{
    int is_static;         //请求的是否为静态文件
    struct stat sbuf;      //描述文件属性的结构,用来获取文件的信息

    char buf[MAXLINE];     //存放每一次读取的数据(一般是一行)
    char method[MAXLINE];  //存放请求方法 "GET"
    char uri[MAXLINE];     //存放URL链接 "http://www.baidu.com/
    char version[MAXLINE]; //存放http版本号 "HTTP/1.1\r\n"
    char filename[MAXLINE];//请求的文件路径
    char cgiargs[MAXLINE]; //CGI参数(动态网页具备的)

    char line[MAXLINE];

    /* 客户已经关闭了连接 */
    if (false == read())
    {
        return STATUS_CLOSE;
    }

    mylog("processRead() readed");
    mylog("fileInfo_ use count %d", fileInfo_.use_count());

    /* 接下来开始解析读入的数据 */

    getLine(line, MAXLINE);                            //读取一行数据进入buf
    sscanf(line, "%s %s %s", method, uri, version);    //从buf中读入请求方法,URL,HTTP版本号到对应字符数组

    if (strcasecmp(method, "GET"))                     //忽略大小比较字符串 服务器目前只支持GET方法
    {
        clienterror(method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        goto end;
    }

    mylog("processRead() start in readRequestHdrs");
    readRequestHdrs();                                 //处理剩余的请求头部
    mylog("processRead() done readRequestHdrs");


    is_static = parseUri(uri, filename, cgiargs);      //解析URL得到请求的文件路径和相关参数

    if (stat(filename, &sbuf) < 0)                                  //将filename路径的文件信息加载进入sbuf中
    {
        clienterror(filename, "404", "Not found",
                    "Tiny couldn't find this file"); /* 没有找到文件 */
        goto end;
    }

    /*请求内容为静态内容*/
    if (is_static)
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))   //该请求内容不为常规文件或者文件不可读
        {
            clienterror(filename, "403", "Forbidden",
                        "Tiny couldn't read the file"); //权限不够
            goto end;
        }
        //静态网页处理函数
        serveStatic(filename, sbuf.st_size);

        mylog("fileInfo_ use count %d", fileInfo_.use_count());

    }
    /* 请求内容为动态内容 */
    else
    {
        clienterror(method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        goto end;

        // not use!
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program"); /* 访问某个程序 */
            goto end;
        }
        serveDynamic(filename, cgiargs);
    }
end:
    setState(kWrite);

    mylog("fileInfo_ use count %d", fileInfo_.use_count());
    mylog("begin write!\n");

    return processWrite();
}

/* 要获取的内容为动态内容 */
void HttpHandle::serveDynamic(char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* 构造响应头部 将响应头部写入 写缓冲区 */
    addResponse("HTTP/1.0 200 OK\r\n");
    addResponse("Server: Tiny Web Server\r\n");
    addResponse("\r\n");

    return;//服务器不支持动态内容...... 以后再改进

    /*开个子进程为客户端运行CGI程序*/
    if (Fork() == 0)   //当前进程为子进程
    {

        /* 真实的服务器会在这里设置所有的cgi变量值 */
        setenv("QUERY_STRING", cgiargs, 1);    //根据CGI设置环境变量值
        Dup2(sockfd_, STDOUT_FILENO);               //重定向到客户端
        Execve(filename, emptylist, environ);  //运行CGI程序
    }

    Wait(NULL);//父进程等待回收子进程资源 防止僵尸进程
}

//初始化
void HttpHandle::init(int sockfd)
{
    sockfd_ = sockfd; /* 记录下连接的socket */

    /* 如下两行是为了避免TIME_WAIT状态,仅仅用于调试,实际使用时应该去掉 ,因为不安全！！！*/
    int reuse = 1;
    Setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); /* 设置端口重用 */

    reset();
}

//重置函数
void HttpHandle::reset()
{
    nRead_ = 0;
    nChecked_ = 0;
    nStored_ = 0;
    keepAlive_ = false;
    sendFile_ = false;
    written_ = 0;

    setState(kRead); /* 现在需要读数据 */
    memset(readBuf_, 0, READ_BUFFER_SIZE);
    memset(writeBuf_, 0, WRITE_BUFFER_SIZE);
}

//读取 读缓冲区 的数据
bool HttpHandle::read()
{
    /* 由于epoll设置成了是边缘触发,所以要一次性将数据全部读尽 */
    /* 首先要清零 */
    nRead_ = 0;
    nChecked_ = 0;

    //清零之后此判断无用 留下了的目的是epoll换成ET模式时 沿用
    if (nRead_ >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int byte_read = 0;
    while (true)
    {
        byte_read = recv(sockfd_, readBuf_ + nRead_, READ_BUFFER_SIZE - nRead_, 0);//接收数据,返回的是已经接收的字节数

        mylog("byte_read:%d", byte_read);

        if (byte_read == -1)    /* 代表出错了 */
        {
            break;
        }
        else if (byte_read == 0)   /* 对方已经关闭了连接 */
        {
            return false;
        }

        nRead_ += byte_read;
    }
    return true;
}

int HttpHandle::processWrite()
{
    int res;
    /*-
    * 数据要作为两部分发送,第1步,要发送writeBuf_里面的数据.
    */
    mylog("send file! fileInfo_ use count %d", fileInfo_.use_count());

    int nRemain = strlen(writeBuf_) - written_; /* writeBuf_中还有多少字节要写 */

    if (nRemain > 0)
    {

        while (true)
        {

            nRemain = strlen(writeBuf_) - written_;
            res = write(sockfd_, writeBuf_ + written_, nRemain);//将 写缓冲区中 数据发送给客户

            if (res < 0)
            {
                if (errno == EAGAIN)   /* 资源暂时不可用 */
                {
                    return STATUS_WRITE;
                }
                return STATUS_ERROR;
            }
            written_ += res;
            //写缓冲区中的数据 发送完了
            if (written_ == strlen(writeBuf_))
                break;
        }
    }

    /*-
    * 第2步,要发送html网页数据.
    */
    if (sendFile_)
    {
        assert(fileInfo_);
        mylog("--> fileInfo_ is not null!");
        mylog("--> fileInfo_ use count %d", fileInfo_.use_count());

        int bytesToSend = fileInfo_->size_ + strlen(writeBuf_); /* 总共需要发送的字节数目 */
        mylog("need bytesToSend! %d", bytesToSend);

        char *fileAddr = (char *)fileInfo_->addr_;
        mylog("fileAddr %d", fileAddr);

        size_t fileSize = fileInfo_->size_;
        mylog("fileSize %d", fileSize);

        while (true)
        {

            int offset = written_ - strlen(writeBuf_);//已经发送的 文件数据字节数

            res = write(sockfd_, (char *)fileInfo_->addr_ + offset, fileInfo_->size_ - offset);//发送文件
            mylog("file send once!");

            if (res < 0)
            {
                if (errno == EAGAIN)   /* 资源暂时不可用 */
                {
                    return STATUS_WRITE;
                }
                return STATUS_ERROR;
            }

            written_ += res;

            //文件发送完毕
            if (written_ == bytesToSend)
                break;
        }

    }

    mylog("send over!");
    /* 所有数据发送完毕 */
    if (keepAlive_)   /* 如果需要保持连接的话 */
    {
        reset();
        return STATUS_READ;
    }
    else
    {
        reset();
        return STATUS_SUCCESS;
    }
}

//将回复的未知个数据参数添加到写缓冲区
//.....代表数据数量未知
bool HttpHandle::addResponse(const char* format, ...)
{
    //写缓冲区已满
    if (nStored_ >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;//个数可变的参数列表
    va_start(arg_list, format);//参数映射放入

    int len = vsnprintf(writeBuf_ + nStored_, WRITE_BUFFER_SIZE - 1 - nStored_, format, arg_list);//写入 写缓冲区

    //写入字符过多,数组爆了
    if (len >= (WRITE_BUFFER_SIZE - 1 - nStored_))
    {
        return false;
    }

    nStored_ += len;
    va_end(arg_list);//清除可变参数列表
    return true;
}

//写入一个字符串进入 写缓冲区
bool HttpHandle::addResponse(char* const str)
{
    int len = strlen(str);

    //写入字符串长度过长
    if ((nStored_ >= WRITE_BUFFER_SIZE) || (nStored_ + len >= WRITE_BUFFER_SIZE))
    {
        return false;
    }

    strncpy(writeBuf_ + nStored_, str, len); /* 拷贝len个字符 */

    nStored_ += len; /* nStored_是写缓冲区的末尾 */
    return true;
}


//请求处理主函数
void HttpHandle::process()
{
    int res;
    switch (state_)
    {
    case kRead:
    {
        res = processRead();

        mylog("processRead res is %d", res);

        if (res == STATUS_WRITE)
            modfd(epollfd_, sockfd_, EPOLLOUT);
        else
            removefd(epollfd_, sockfd_);
        break;
    }
    case kWrite:
    {
        res = processWrite();

        mylog("processWrite res is %d", res);

        if (res == STATUS_READ)
            modfd(epollfd_, sockfd_, EPOLLIN);
        else
            removefd(epollfd_, sockfd_);
        break;
    }
    default:
        removefd(epollfd_, sockfd_);
        break;
    }
}
