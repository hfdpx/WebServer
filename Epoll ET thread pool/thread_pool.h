#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <list>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <utility>
#include "mutex.h"
#include "condition.h"
#include "csapp.h"
#include "noncopyable.h"
#include "epoll_ulti.h"

class ThreadPool : noncopyable
{
public:
    //执行任务
    //boost::function可以用来代替拥有相同返回类型，相同参数类型，以及相同参数个数的各个不同的函数
    typedef boost::function<void()> Task;

    //构造函数
    ThreadPool(int threadNum, int maxTaskNum);

    //析构函数
    ~ThreadPool();

    //添加任务到工作队列中 生产者
    //使用了右值引用
    bool append(Task&&);

    //线程池运行函数
    void run();

    //所有线程的运行时函数
    static void* startThread(void* arg);
private:

    //判断工作队列是否空
    bool isFull();

    //从任务队列中获取任务
    Task take();

    //返回任务队列中任务个数
    size_t queueSize();

    //线程池中线程数量(阻塞或正在运行的)
    int threadNum_;

    //任务队列可以容纳的最大任务数
    int maxQueueSize_;

    //任务队列
    std::list<Task> queue_;

    //任务队列的锁          //任务队列可以考虑实现分段锁,头尾两把锁
    MutexLock mutex_;

    //任务队列不空 条件变量
    Condition notEmpty_; //给头部锁

    //任务队列不满 条件变量
    Condition notFull_;// 给尾部锁
};

#endif /* _THREAD_POOL_H_ */
