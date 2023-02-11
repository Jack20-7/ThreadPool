# SimpleThreadPool

## ThreadPool-CPP
基于c++11实现的简单线程池
优点:
    1.使用简单，不易出错
    2.支持线程复用，提高性能
    3.支持懒惰创建线程
    4.必要的时候可以自动空闲线程
example:
```
  ThreadPool pool(4);
  
  auto result = pool.enqueue([](int val) { return val;},20);
  
  std::cout << "result = " << result.get() << std::endl;
```

## ThreadPool-C
使用C语言实现的简单线程池
