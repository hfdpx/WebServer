# A C++ High Performance Web Server  
## Introduction 
本项目为C++11编写的轻量级高性能的Web服务器，解析了get，head请求，可处理静态资源，支持HTTP长连接  

测试页:[http://47.93.39.164:8080/](http://47.93.39.164:8080/)

# Envoirment  
* OS:CentOS 7.2
* Complier:GCC 4.8.5

# Bulid
     make
    ./web_final

## Technical points
* 使用Epoll边沿触发的IO多路复用技术，使用Reactor模式构建程序框架

* 使用多线程充分利用多核CPU，并使用线程池避免线程频繁创建销毁的开销

* 使用生产者消费者模型处理任务

* 使用Cache机制，将用户群体经常访问的页面放入Cache中，减少磁盘读写次数和保证响应速度

* 使用LRU算法淘汰Cahce中的文件，保证页面命中率  

* 使用Mutex互斥锁和condition条件变量保证线程安全

* 使用Segtion lock分段锁机制提高多线程运行效率

* 使用状态机机制解析Http请求，保证用户的所有合法请求都能被处理

* 为减少内存泄漏的可能，采用了智能指针

* 构建了一个高效的buffer，支持自适应扩容，内存连续，在必要时可利用临时的栈上空间暂存数据

* 使用双向循环缓冲区链表+日志线程构建了异步日志库，能记录服务器运行时的状态，一秒钟可用写百万条日志到磁盘！

## Wait to do   

* 使用基于小根堆的定时器关闭超时请求

* 支持优雅的关闭连接