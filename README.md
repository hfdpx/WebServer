# Web-Server

### 版本1：单进程+BIO 版本的web server!  
  
  当没有连接到来的时候,该进程会阻塞在Accept函数上,  
  当有多个连接到来的时候，只能处理一个连接，其他连接进入阻塞队列等待，  
  
  当然,这里的connfd也是阻塞版本的！也就是说,在对connfd执行write,read等函数的时候也会阻塞.  
  比如一个连接要先reda资源然后write资源，如果要reda的时候服务器没有准备好，那么系统会阻塞处理该连接的进程，等待要read的资源准备好  
  
  这样一来,这个版本的server效率会非常低下！
  
  
  
### 版本2：多进程+BIO 版本的web server!

  * 比单进程+BIO版本的server效率有所提高  

  当没有连接到来的时候,该进程会阻塞在Accept函数上,  
  当有多个连接到来的时候，为每一个连接fork一个进程  

  当然,这里的connfd也是阻塞版本的！也就是说,在对connfd执行write,read等函数的时候也会阻塞.  
  比如一个连接要先reda资源然后write资源，如果要reda的时候服务器没有准备好，那么系统会阻塞处理该连接的进程，等待要read的资源准备好  

  因为是BIO的，每个进程可能因为连接没有请求到相应的资源而阻塞

  这样一来，多进程+BIO版本的server效率仍然不是很高！


### 版本3:多线程+BIO 版本的web server!
  
  * 和多进程+BIO版本的web server各有千秋
  * 其效率取决于server的业务和物理机器的硬件资源  
  
跟多进程+BIO的版本相差不大,都是为每个新的连接建立一个进程或者线程,下一版本可以考虑采用线程池降低线程创建和销毁的开销


#### ps:关于多进程和多线程的对比
###### “鱼和熊掌不可兼得”  
  * 数据的共享与同步：
    * 多进程数据共享复杂，需要用IPC，数据是分开的，同步也简单
    * 多线程共享进程数据，所以数据共享简单，但同步复杂
  * 内存和CPU占比：
    * 多进程占用内存多，切换复杂，CPU利用率低
    * 多线程占用内存少，切换简单，CPU利用率高  
  * 创建和销毁：
    * 进程的创建，销毁和切换复杂，速度慢
    * 线程的创建，销毁和切换简单，速度快
  * 可靠性：  
    * 进程间不会互相影响，一个进程的崩溃不会影响其他进程
    * 一个线程挂掉将导致整个进程挂掉
  * 分布式：
    * 适用于多核，多机分布式，如果一台机器不够，扩展到多台机器简单
    * 只使用于多核分布式
  * 编程和调试：  
    * 多进程编程和调试简单
    * 多线程编程和调试复杂
###### 写Web Server个人倾向于使用多线程


    










