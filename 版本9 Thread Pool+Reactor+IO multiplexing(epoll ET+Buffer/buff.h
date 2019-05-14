#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <algorithm>
#include <vector>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <sys/socket.h>

class Buffer
{
public:
    //初始化prepend为8个字节大小 方便以后在头部追加数据
    static const size_t kCheapPrepend = 8;

    //初始化Buffer大小为1024字节
    static const size_t kInitialSize = 1024;

    //Buffer 构造函数
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {

    }
    // 不修改类的成员变量的值的函数都要用const修饰,这是一种好的习惯
    size_t readableBytes() const   /* 可读的字节数目 */
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const   /* 可写的字节数目 */
    {
        return buffer_.size() - writerIndex_;
    }

    //计算prependable大小
    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回数据内容的起始位置
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    //寻找第一个'\n'位置
    const char* findEOL() const
    {
        const void* eol = memchr(peek(), '\n', readableBytes());
        return static_cast<const char*>(eol);
    }

    //寻找\r\n 位置*
    const char* findCRLF() const
    {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    bool getLine(char *dest, size_t len); /* 读取一行数据 */

    const char* findEOF() const
    {
        const void* eol = memchr(peek(), '\n', readableBytes());
        return static_cast<const char*>(eol);
    }

    const char* findEOL(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const void* eol = memchr(start, '\n', beginWrite() - start);
        return static_cast<const char*>(eol);
    }

    //该函数用来回收Buffer空间,在读取Buffer内容之后,调用此函数来挪动索引
    void retrieve(size_t len)
    {
        //mylog("len = %d, reableBytes() = %ld", len, readableBytes());

        assert(len <= readableBytes());
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            //当要回收的大小超过可读数目,直接回收所有空间
            retrieveAll();
        }
    }

    //回收所有Buffer空间,将两个索引回归到初始位置
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    //保证Buffer有足够的空间写数据
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    //写入数据
    void  append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        hasWritten(len);
    }

    //格式化写入
    void appendStr(const char* format, ...);

    //写入起始点
    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const   /* const版本 */
    {
        return begin() + writerIndex_;
    }

    //已经写完,挪动索引
    void hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    //撤销
    void unwrite(size_t len)
    {
        assert(len <= readableBytes());
        writerIndex_ -= len;
    }

    //数据追加在头部
    void prepend(const void* data, size_t  len)
    {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char *d = static_cast<const char*>(data);
        std::copy(d, d + len, begin() + readerIndex_);
    }

     //Buffer的容量
    size_t internalCapacity() const
    {
        return buffer_.capacity();
    }

    //溢出的数据存放在临时栈 然后等buffer扩容完成后又搬回来
    ssize_t readFd(int fd, int* savedErrno);
private:
    char* begin()
    {
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }

    //制造空间:扩容 或者 挪动
    void makeSpace(size_t len)
    {
        //剩余空间不够了
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            //resize进行扩容
            buffer_.resize(writerIndex_ + len);
        }
        //如果剩余的空间大小足够了!
        //数据挪到buffer前面,因为前面prependable还要富裕空间
        else
        {
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes(); /* 可读的字节数目 */
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);//挪动

             //标识更新
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;

            assert(readable == readableBytes());
        }
    }
private:
    //使用vector来存储字符
    std::vector<char> buffer_;

    //读指示器,采取下标的而不是迭代器的形式，是因为迭代器会失效
    size_t readerIndex_;

    //写指示器
    size_t writerIndex_;

    // \r\n
    static const char kCRLF[];
};

#endif
