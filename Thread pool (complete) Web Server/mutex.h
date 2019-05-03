#ifndef _MUTEX_H_
#define _MUTEX_H_
/*-
 * 参照muduo库的mutex写出的一个简易的Mutex类.
 */

#include <assert.h>
#include <pthread.h>
#include "noncopyable.h"


/**实现堆内存的一致性状态检测**/
/*让程序在堆内存越界时就立刻core掉以便及时保留现场,避免对内存越界导致不可预知的错误*/
#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       if (__builtin_expect(errnum != 0, 0))    \
                            __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

/**互斥锁类的封装**/
class MutexLock : noncopyable//不可拷贝类
{
public:
	MutexLock()/*构造函数*/
		    : holder_(0)//表示互斥量上锁线程的tid
	{
		MCHECK(pthread_mutex_init(&mutex_, NULL)); /* 互斥锁的初始化 */
	}

	~MutexLock()/*析构函数*/
	{
		assert(holder_ == 0);/* 断言函数,条件为假则终止程序运行 */
		MCHECK(pthread_mutex_destroy(&mutex_)); /* 销毁锁 */
	}

	bool isLockedByThisThread() const /* 测试锁是否被本线程持有 */
	{
		return holder_ == pthread_self();
	}

	void assertLocked() const
	{
		assert(isLockedByThisThread());/* 锁不被本线程持有则退出当前线程*/
	}

	void lock()
	{
		MCHECK(pthread_mutex_lock(&mutex_));/*当前线程获得锁*/
		/*顺序不能反,只有当前线程已经获得了锁才能确定当前线程为锁的拥有者*/
		assignHolder(); /*将拥有锁的线程号设置为当前线程 */
	}

	void unlock()
	{
		unassignHolder(); /* 将锁的拥有者设置为空 */
		MCHECK(pthread_mutex_unlock(&mutex_));/*当前线程释放锁*/
	}

	pthread_mutex_t* getPthreadMutex()/*返回锁地址*/
	{
		return &mutex_;
	}
private:
	friend class Condition;/*同步机制(条件变量)封装类*/

	/**这个类的作用是方便安全的改变锁的持有者**/
	class UnassignGuard : noncopyable
	{
	public:
		UnassignGuard(MutexLock& owner)/*构造函数*/
			: owner_(owner)
		{
			owner_.unassignHolder();/*将锁的拥有者设置为空*/
		}

		~UnassignGuard()/*析构函数*/
		{
			owner_.assignHolder();/*将拥有锁的线程号设置为当前线程*/
		}
	private:
		MutexLock& owner_;/*持有锁的引用*/
	};

	void unassignHolder()/*对holder置0*/
	{
		holder_ = 0;
	}
	void assignHolder()/*给holder赋值为当前线程id*/
	{
		holder_ = pthread_self();
	}

	pthread_mutex_t mutex_;/*互斥锁*/

	pid_t holder_;/*持有锁的线程id*/
};


/**对锁进行加锁,解锁操作,RAll机制的体现**/
class MutexLockGuard : noncopyable//不可拷贝类
{
public:
	explicit MutexLockGuard(MutexLock& mutex)
		: mutex_(mutex)
	{
		mutex_.lock();/*构造时加锁*/
	}
	~MutexLockGuard()
	{
		mutex_.unlock();/*析构时解锁*/
	}
private:
	MutexLock& mutex_;/*持有锁的一个引用*/
};


#endif /* _MUTEX_H_ */


/*


知识点1:

Muduo网络库中使用RAll机制封装了同步原语，自己实现了互斥锁和条件变量，现在记录一下

其中最主要的几个类有:

MutexLock和Condtion:对线程库中互斥锁和条件变量的封装

MutexLockGuard和CountDownLatch:对上面两个的封装

*/

/*
知识点2:

RAll机制:

"资源获取即初始化"

其核心是把资源和对象的生命周期绑定,对象创建获取资源,对象销毁释放资源


*/
