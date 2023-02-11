#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include<stdio.h>

#include<unordered_map>
#include<queue>
#include<memory>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<future>
#include<functional>
#include<stdexcept>
#include<type_traits>
#include<cassert>
#include<chrono>

/* 该线程池的优点:
   1.使用简单，不易出错
   2.支持线程复用，提高性能
   3.支持懒惰创建线程
   4.必要的是否自动回收空闲线程
*/
class ThreadPool{
public:
    explicit ThreadPool(size_t);

    //禁止拷贝操作
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator= (const ThreadPool&) = delete;

    //result_of关键字可以用来获取可调用对象的返回值
    //future提供了访问异步操作结果的机制
    template<class F,class... Args>
    auto enqueue(F&& f,Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

    size_t threadsNum()const{
       return currentThreads;
    }
    ~ThreadPool();
private:
    static constexpr size_t WAIT_SECONDS = 2;       //工作线程的超市时间,默认是10s
    void worker();                   //线程的工作函数
    void joinFinishedThreads();      //慢慢回收线程
    //std::vector<std::thread> workers;
    std::queue<std::function<void ()> > tasks;                 //工作队列
    std::queue<std::thread::id> finishedThreads;               //存放可复用的线程的id
    std::unordered_map<std::thread::id,std::thread> threads;   //管理创建的线程

    size_t currentThreads;                      //当前线程数目
    size_t idleThreads;                         //空闲线程数目
    size_t maxThreads;                          //最大线程数目

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

constexpr size_t ThreadPool::WAIT_SECONDS;

inline ThreadPool::ThreadPool(size_t threads)
    :currentThreads(0),
     idleThreads(0),
     maxThreads(threads),
     stop(false){
    //for(size_t i = 0;i < threads;++i){
    //    workers.emplace_back(
    //        [this]
    //         {
    //           for(;;){
    //             std::function<void()> task; 
    //             {
    //               //这个unique_lock就相当于lockGuard
    //               std::unique_lock<std::mutex> lock(this->queue_mutex);
    //               //讲while循环的条件整合到了wait里面去,当被唤醒时，如果后面那个可调用对象的返回值=false，就继续wait,一直直到被唤醒时并且可调用对象的返回值=true，才能够跳出去
    //               this->condition.wait(lock,
    //                   [this]{ return this->stop || !this->tasks.empty();} );
    //               if(this->stop || this->tasks.empty()){
    //                   return ;
    //               }
    //               task = std::move(this->tasks.front());
    //               this->tasks.pop();
    //             }
    //             task();
    //           }
    //         }
    //    );
    //}
}

//添加新任务到工作队列
template<class F,class... Args>
auto ThreadPool::enqueue(F&& f,Args&&... args)
    ->std::future<typename std::result_of<F(Args...)>::type>{
    //typedef typename std::result_of<F(Args...)>::type> return_type
    using  return_type = typename std::result_of<F(Args...)>::type;
    //packaged_task是一个类模板，可以用来保存任何的可调用对象(函数、函数指针、lambda表达式、函数对象),以便他被异步调用
    auto task = std::make_shared<std::packaged_task<return_type()>> (
        std::bind(std::forward<F>(f),std::forward<Args>(args)...)  //这个属于是参数，所以末尾不加分号
    );
    std::future<return_type> res = task->get_future();
    {
       std::unique_lock<std::mutex> lock(queue_mutex);
       if(stop){
         throw std::runtime_error("enqueue on stopped ThreadPool");
       }
       //packaged_task不会自己自动，需要我们显式的进行调用
       tasks.emplace([task](){ (*task)();});

       if(idleThreads > 0){
          condition.notify_one();
       }else if(currentThreads < maxThreads){
          //当前没有空闲的线程，但是线程池的线程数未达到最大值，则可以动态创建线程
          std::thread t(&ThreadPool::worker,this);
          assert(threads.find(t.get_id()) == threads.end());
          threads[t.get_id()] = std::move(t);
          ++currentThreads;
       }
    }
    return res;
}


void ThreadPool::joinFinishedThreads(){
    while(!finishedThreads.empty()){
        auto id = std::move(finishedThreads.front());
        finishedThreads.pop();
        auto it = threads.find(id);

        assert(it != threads.end());
        assert(it->second.joinable());

        it->second.join();
        threads.erase(it);
    }
}

void ThreadPool::worker(){
   while(true){
      std::function<void ()> task;
      {
         std::unique_lock<std::mutex> lock(queue_mutex);
         ++idleThreads;
         //wait_for的条件就相当于线程退出条件变量wait的条件
         auto hasTimedout = !condition.wait_for(lock,std::chrono::seconds(WAIT_SECONDS),
                                                         [this](){
                                                          return stop || !tasks.empty();
                                                                 });
         --idleThreads;
         if(tasks.empty()){
            if(stop){
              --currentThreads;
              return ;
            }
            if(hasTimedout){
               //如果都超时了还没有任务，那么就会将慢慢的移除线程池中的线程
               --currentThreads;
               joinFinishedThreads();
               finishedThreads.emplace(std::this_thread::get_id());
               return ;
            }
         }

         task = std::move(tasks.front());
         tasks.pop();
      }
      task();
   }
}

inline ThreadPool::~ThreadPool(){
   printf("~ThreadPool start\n");
   {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
   }
   printf("notify_all\n");
   condition.notify_all();
   for(auto& item : threads){
       assert(item.second.joinable());
       item.second.join();
   }
   printf("~ThreadPool end\n");
}

#endif
