#include"ThreadPool.h"

#include<iostream>
#include<vector>
#include<chrono>
#include<stdio.h>


int main(void){
    ThreadPool pool(8);
    std::vector<std::future<int>> results;

    for(int i = 0;i < 8;++i){
        results.push_back(
           pool.enqueue(
               [i]{
                 //std::cout << "hello " << i << std::endl;    
                 printf("hello %d\n",i);
                 printf("world %d\n",i);
                 //std::cout << "world " << i << std::endl;
                 return i * i;
               }
            ) 
        );
    }
    printf("function push finished\n");
    //用来获取执行的结果
    for(auto&& result : results){
        printf("%d ",result.get());
    }
}
