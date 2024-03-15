# ThreadPool介绍
线程池是一种用于管理和调度线程的机制，它能够有效地重用线程、减少线程创建和销毁的开销，并且能够控制并发线程的数量。
第一版是基于C++11新特性实现的线程池，手动实现c++17的any类型c++20的Semaphore类型等。

# Linux下编译成动态库
  1.生成动态库
  g++ -fPIC -shared threadpool.cpp -o libtdpool.so -std=c++17
  2.移动动态库到系统目录下
  mv libtdpool.so /usr/local/lib
  3.删掉threadpool.cpp文件，只提供动态库
  rm -rf threadpool.cpp
  4.移动头文件到系统库目录下
  mv threadpool.h  /usr/local/include
  5.生成可执行文件
  g++ test_program.cpp -std=c++17 -ltdpool -lpthread
  6.编译时与运行时寻找动态库的方式不一样，所以在运行./a.out时会报错,解决办法如下：
  cd /etc/ld.so.conf.d/  
  创建mylib.conf文件（文件名随意）在文件中写入动态库的路径  /usr/local/lib
  ldconfig   //作用：刷新配置路径到 /etc/ld.so.cache中
    
  
# 线程池作用
提高性能：线程池可以重复利用已创建的线程，避免了频繁创建和销毁线程的开销，从而提高了程序的性能。

控制并发度：线程池可以限制系统中并发线程的数量，防止过多的线程同时执行，从而控制并发度，避免资源过度占用和系统崩溃。

提高响应速度：线程池能够立即分配可用线程来处理任务，而不需要等待新线程的创建，从而提高了任务的响应速度。

避免资源竞争：线程池可以通过合理的任务队列和调度策略，避免多个线程同时访问共享资源导致的竞争问题，提高了程序的稳定性和可靠性。

# 线程池应用
高并发网络服务器
master-slave线程模型
耗时任务处理
后台任务处理

