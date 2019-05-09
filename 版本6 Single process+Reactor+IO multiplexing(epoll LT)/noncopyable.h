#ifndef _NON_COPYABLE_H_
#define _NON_COPYABLE_H_

/**声明一个类不允许复制时直接继承noncopyable*/
class noncopyable
{
protected://！！！
    noncopyable() {} /* 这里主要是允许派生类对象的构造和析构 */
    ~noncopyable() {}
private://！！！
    noncopyable(const noncopyable&); /* 拷贝构造函数 */
    const noncopyable& operator=(const noncopyable&); /* 赋值操作符 */
};

#endif /* _NON_COPUABLE_H_ */

/*
关于不可复制类原理的说明:

如果用户没有显示定义复制构造函数和赋值操作符,编译器将会默认的合成一个复制构造函数和赋值操作符

为了防止复制和赋值，类可以显示声明其构造函数和赋值操作符为private

如果拷贝构造函数是私有的，将不允许用户代码复制该类型的对象，编译器将拒绝进行任何复制尝试

*/
