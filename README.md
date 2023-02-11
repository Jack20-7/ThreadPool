# SimpleThreadPool
基于c++11实现的简单线程池

example:

```
  ThreadPool pool(4);
  
  auto result = pool.enqueue([](int val) { return val;},20);
  
  std::cout << "result = " << result.get() << std::endl;
```
