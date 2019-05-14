#ifndef _CONDITION_H_
#define _CONDITION_H_

#include <pthread.h>
#include "mutex.h"
#include "noncopyable.h"

/*-
* 参照muduo库的condition写的一个简易的condition类.
*/
/**同步机制的封装**/
class Condition : noncopyable
{
public:
	explicit Condition(MutexLock& mutex)
		: mutex_(mutex)
	{
		MCHECK(pthread_cond_init(&pcond_, NULL)); /* 构造时初始化 */
	}

	~Condition()
	{
		MCHECK(pthread_cond_destroy(&pcond_));    /* 析构时销毁条件变量 */
	}

	void wait()
	{
		MutexLock::UnassignGuard ug(mutex_);
		MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex())); /* 等待Mutex */
	}

	void notify()
	{
		MCHECK(pthread_cond_signal(&pcond_));     /* 唤醒一个线程 */
	}
	void notifyAll()
	{
		MCHECK(pthread_cond_broadcast(&pcond_));   /* 唤醒多个线程 */
	}
private:
	MutexLock& mutex_;
	pthread_cond_t pcond_;
};

#endif
/*
关于几个函数的说明:

1.pthread_cond_wait(
                   pthread_cond_t *cond ,
                   pthread_mutex_t*mutex
                   );
为参数mutex指定的互斥体解锁,等待一个事件发生(cond指定的条件变量)

调用该函数的线程被阻塞,知直到有其他线程调用相应的函数改变条件变量，而且获得mutex互斥体时才解除阻塞

2.pthread_cond_signal(
                    pthread_cond_t *cond
                    );
解除cond参数指定的条件变量的一个线程的阻塞状态,当有多个线程阻塞等待该条件变量时，也只唤醒一个线程

3.pthread_cond_broadcast(
                       pthread_cond_t *cond
                       );
解除cond参数指定的条件变量的所有线程的阻塞状态

*/

