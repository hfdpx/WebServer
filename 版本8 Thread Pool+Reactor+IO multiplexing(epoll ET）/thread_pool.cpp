#include "thread_pool.h"
#include "mutex.h"


/*构造函数*/
ThreadPool::ThreadPool(int threadNum, int maxQueueSize)
//线程数量
    : threadNum_(threadNum)

    //最大任务数
    , maxQueueSize_(maxQueueSize)

    //任务队列锁
    , mutex_()

    //任务队列不空 条件变量
    , notEmpty_(mutex_)
    //任务队列不满 条件变量
    , notFull_(mutex_)

{
    //确保程序正确运行
    assert(threadNum >= 1 && maxQueueSize >= 1);

    //接下来构建threadNum个线程
    pthread_t tid_t;
    for (int i = 0; i < threadNum; i++)
    {
        Pthread_create(&tid_t, NULL, startThread, this);
    }
}

size_t ThreadPool::queueSize()
{

    MutexLockGuard lock(mutex_);//获得任务队列锁

    return queue_.size();
}

ThreadPool::~ThreadPool()
{

}

//线程运行时函数
void* ThreadPool::startThread(void* obj)
{
    //子线程和主线程分离 以便子线程的资源回收
    Pthread_detach(Pthread_self());

    //显式类型转换
    ThreadPool* pool = static_cast<ThreadPool*>(obj);

    //回调运行线程池运行函数
    pool->run();
    return pool;
}

//获得任务
ThreadPool::Task ThreadPool::take()
{

    MutexLockGuard lock(mutex_);//获得任务队列锁

    //若任务队列为空，则阻塞等待
    while (queue_.empty())
    {
        notEmpty_.wait();
    }

    Task task;

    //获得任务
    if (!queue_.empty())
    {
        //获得队列首部的任务
        task = queue_.front();
        queue_.pop_front();

        //通知线程池任务队列不满,以便添加任务
        if (maxQueueSize_ > 0)
        {
            notFull_.notify();
        }
    }
    mylog("threadpool take 1 task!");
    return task;
}

//线程池运行函数
void ThreadPool::run()
{
    for ( ; ; )   /* 一直运行下去 */
    {
        //获得任务以及获得执行该任务的执行函数
        Task task(take());

        //该任务的执行函数存在
        if (task)
        {
            mylog("task run!");

            //执行任务
            task();
        }
        mylog("task over!");
    }
}

//判断任务队列是否满
bool ThreadPool::isFull()
{
    mutex_.assertLocked();//锁不为本线程持有则退出当前线程

    return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

//添加任务 到 任务队列
bool ThreadPool::append(Task&& task)
{
    /* 使用了右值引用 */
    {
        MutexLockGuard lock(mutex_); //获得任务队列锁

        while (isFull())
        {
            notFull_.wait(); /* 等待queue有空闲位置 */
        }
        //确保程序健壮性
        assert(!isFull());

        //任务加入任务队列
        queue_.push_back(std::move(task)); /* 直接用move语义,提高了效率 */

        mylog("put task onto queue!");
    }

    notEmpty_.notify(); /* 通知任务队列中有任务可做了 */
}
