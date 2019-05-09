#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "csapp.h"
#include "epoll_ulti.h"
#include "http_server.h"


/* 网页的根目录 */
const char * root_dir = "/home/yb/WebSiteSrc/html_book_20150808/reference";
/* / 所指代的网页 */
const char * home_page = "index.html";

typedef struct condition
{
    pthread_mutex_t pmutex;//互斥锁
    pthread_cond_t pcond;//条件变量
} condition_t;

typedef struct task//任务结构体
{
    void *(*run)(void *arg);
    void *arg;
    struct task *next;//指向下一个任务
} task_t;

typedef struct threadpool//线程池
{
    condition_t ready;

    task_t *first;//队首任务

    task_t *last;//队尾任务

    int counter;//当前运行的线程数量

    int idle;//阻塞线程数量

    int max_threads;//线程池中最大线程数量

    int quit;//线程池销毁标志,0:销毁
} threadpool_t;


int condition_init(condition_t *cond)
{
    int status;
    if((status = pthread_mutex_init(&cond->pmutex,NULL)))//互斥锁初始化
        return status;
    if((status = pthread_cond_init(&cond->pcond,NULL)))//条件变量初始化
        return status;
    return 0;
}
int condition_lock(condition_t *cond)//线程加锁
{
    return pthread_mutex_lock(&cond -> pmutex);
}
int condition_unlock(condition_t *cond)//线程解锁
{
    return pthread_mutex_unlock(&cond -> pmutex);
}
int condition_wait(condition_t *cond)//基于条件变量阻塞，无条件等待
{
    return pthread_cond_wait(&cond -> pcond,&cond -> pmutex);
}
int condition_timewait(condition_t *cond,const struct timespec *abstime)//基于一定时间的阻塞
{
    return pthread_cond_timedwait(&cond->pcond,&cond->pmutex,abstime);
}
int condition_signal(condition_t *cond)//唤醒一个阻塞线程
{
    return pthread_cond_signal(&cond->pcond);
}
int condition_broadcast(condition_t *cond)//唤醒所有阻塞线程
{
    return pthread_cond_broadcast(&cond -> pcond);
}

//摧毁condition
int condition_destory(condition_t *cond)
{
    int status;
    if((status = pthread_mutex_destroy(&cond -> pmutex)))
        return status;
    if((status = pthread_cond_destroy(&cond -> pcond)))
        return status;
    return 0;
}

//线程运行函数
void *thread_routine(void *arg)
{
    struct timespec abstime;//时间结构体(秒和纳秒)

    int timeout;//超时标志
    printf("thread 0x%0x is starting\n",(int)pthread_self());//输出当前运行的线程

    threadpool_t *pool = (threadpool_t *)arg;//线程池指针

    while(1)
    {
        timeout = 0;
        condition_lock(&pool -> ready);//获得任务队列的锁
        pool -> idle++;

        //等待队列有任务到来或者线程池销毁的通知
        while(pool -> first == NULL && !pool -> quit)
        {
            printf("thread 0x%0x is waiting\n",(int)pthread_self());

            clock_gettime(CLOCK_REALTIME,&abstime);//获取当前系统时间

            abstime.tv_sec += 2;//+2秒

            int status=condition_timewait(&pool -> ready,&abstime);//等待2秒
            if(status == ETIMEDOUT)//超时
            {
                printf("thread 0x%0x is wait timed out\n",(int)pthread_self());
                timeout = 1;
                break;
            }
        }

        pool -> idle--;//存在任务到来,阻塞线程数减1

        //存在任务
        if(pool -> first != NULL)
        {
            task_t *t = pool -> first;//获取队首任务
            pool -> first = t -> next;//指向下一个任务
            //需要先解锁，以便添加新任务。其他消费者线程能够进入等待任务。
            condition_unlock(&pool -> ready);

            t -> run(t->arg);//执行任务
            free(t);//释放任务空间

            condition_lock(&pool -> ready);//重新获得锁
        }

        //任务队列空,但是不需要销毁线程池
        if(pool -> quit && pool ->first == NULL)
        {
            pool -> counter--;//正在运行的线程数量减1
            if(pool->counter == 0)
            {
                condition_signal(&pool -> ready);//唤醒一个阻塞线程
            }
            condition_unlock(&pool->ready);
            //跳出循环之前要记得解锁
            break;
        }

        //任务队列为空，并且线程等待得超时了
        if(timeout &&pool -> first ==NULL)
        {
            pool -> counter--;
            condition_unlock(&pool->ready);
            //跳出循环之前要记得解锁
            break;
        }

        condition_unlock(&pool -> ready);
    }

    printf("thread 0x%0x is exiting\n",(int)pthread_self());
    return NULL;
}

//初始化
void threadpool_init(threadpool_t *pool, int threads)
{
    condition_init(&pool -> ready);
    pool -> first = NULL;
    pool -> last = NULL;
    pool -> counter = 0;
    pool -> idle = 0;
    pool -> max_threads = threads;
    pool -> quit = 0;
}

//加任务
void threadpool_add_task(threadpool_t *pool, void *(*run)(void *arg),void *arg)
{
    //新任务参数配置
    task_t *newstask = (task_t *)malloc(sizeof(task_t));
    newstask->run = run;
    newstask->arg = arg;
    newstask -> next = NULL;

    condition_lock(&pool -> ready);//获得锁

    //将任务添加到对列中
    if(pool -> first ==NULL)
    {
        pool -> first = newstask;
    }
    else
        pool -> last -> next = newstask;
    pool -> last = newstask;


    //如果有阻塞线程，则唤醒其中一个
    if(pool -> idle > 0)
    {
        condition_signal(&pool -> ready);
    }
    //没有阻塞线程且当前线程数量小于线程池允许的最大线程数,则创建一个新线程
    else if(pool -> counter < pool -> max_threads)
    {
        pthread_t tid;
        pthread_create(&tid,NULL,thread_routine,pool);
        pool -> counter++;
    }

    condition_unlock(&pool -> ready);//添加任务完成之后要解锁
}
//销毁线程池
void  threadpool_destory(threadpool_t *pool)
{

    if(pool -> quit)
    {
        return;
    }
    condition_lock(&pool -> ready);//不管怎么样,先获得任务队列的锁
    pool->quit = 1;

    if(pool -> counter > 0)//存在正在运行的线程
    {
        if(pool -> idle > 0)//存在阻塞线程
            condition_broadcast(&pool->ready);//唤醒所有阻塞线程

        while(pool -> counter > 0)//将所有正在运行的线程阻塞
        {
            condition_wait(&pool->ready);
        }
    }
    condition_unlock(&pool->ready);//解锁
    condition_destory(&pool -> ready);//正式开始销毁线程池
}


void *mytask(void *arg)
{
    printf("thread 0x%0x is working on task %d\n",(int)pthread_self(),*(int*)arg);

    int fd = (*(int *)arg);
    Free(arg);

    doit(fd);

    close(fd);

    return NULL;
}

int main(int argc, char *argv[])
{
    int listenfd = Open_listenfd(8080); /* 8080号端口监听 */

    signal(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号

    threadpool_t pool;
    threadpool_init(&pool,3);//线程池初始化

    while(true)
    {
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);

        int *fdp = (int *)Malloc(sizeof(int));
        *fdp = Accept(listenfd, (SA*)&clientaddr, &len);

        threadpool_add_task(&pool,mytask,fdp);
    }
}
