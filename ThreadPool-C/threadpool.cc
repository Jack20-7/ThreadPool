#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

//
struct job{
    void* (* callback)(void* arg);  //工作的回调函数
    void* arg;
    struct job* next;
};

//线程池
struct ThreadPool{
    int thread_num;        //线程池中线程的数目
    int queue_max_num;     //工作队列中最大任务数

    pthread_t* pthreads;
    struct job* head;
    struct job* tail;

    pthread_mutex_t mutex;
    pthread_cond_t queue_empty;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;

    int queue_cur_num;     //当前工作队列的任务数
    int queue_close;
    int pool_close;
};

struct ThreadPool* threadpool_init(int thread_num,int queue_max_num){
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    if(NULL == pool){
        printf("failed to malloc threadpool\n");
        return NULL;
    }

    pool->thread_num = thread_num;
    pool->queue_max_num = queue_max_num;
    pool->queue_cur_num = 0;
    pool->queue_close = 0; 
    pool->pool_close = 0; 
    pool->head = pool->tail = NULL;

    if(pthread_mutex_init(&pool->mutex,NULL)){
        printf("pthread_mutex_init failed\n");
        return NULL;
    }
    if(pthread_cond_init(&pool->queue_empty,NULL)){
        printf("pthread_cond_init queue_empty failed\n");
        return NULL;
    }
    if(pthread_cond_init(&pool->queue_not_empty,NULL)){
        printf("pthread_cond_init queue_not_empty failed\n");
        return NULL;
    }
    if(pthread_cond_init(&pool->queue_not_full,NULL)){
        printf("pthread_cond_init queue_not_full failed\n");
        return NULL;
    }

    pool->pthreads = (pthread_t*)malloc(sizeof(pthread_t) * thread_num);
    if(NULL == pool->pthreads){
        printf("pthreads malloc failed\n");
        return NULL;
    }

    int i = 0;
    for(;i < thread_num;++i){
        pthread_create(&pool->pthreads[i],NULL,threadpool_function,(void*)pool);
    }


    return pool;
}

int                threadpool_add_job(struct ThreadPool* pool,void* (* callback)(void* arg),void* arg){
    assert(pool != NULL);
    assert(callback != NULL);
    assert(arg != NULL);

    pthread_mutex_lock(&pool->mutex);
    while((pool->queue_cur_num == pool->queue_max_num) && !(pool->queue_close || pool->pool_close)){
          pthread_cond_wait(&pool->queue_not_full,&pool->mutex);
    }

    if(pool->queue_close || pool->pool_close){
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    struct job* pjob = (struct job*)malloc(sizeof(struct job));
    if(NULL == pjob){
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    pjob->callback = callback;
    pjob->arg = arg;
    pjob->next = NULL;
    if(pool->head == NULL){
        //如果当前没有任务的话
        pool->head = pool->tail = pjob;
        pthread_cond_broadcast(&pool->queue_not_empty);
    }else{
        pool->tail->next = pjob;
        pool->tail = pjob;
    }

    pool->queue_cur_num++;
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int                threadpool_destroy(struct ThreadPool* pool){
    assert(pool != NULL);
    pthread_mutex_lock(&pool->mutex);
    if(pool->queue_close || pool->pool_close){
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    pool->queue_close = 1;
    while(pool->queue_cur_num != 0){
        pthread_cond_wait(&pool->queue_empty,&pool->mutex);
    }
    pool->pool_close = 1;
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_cond_broadcast(&pool->queue_not_full);

    //销毁线程
    int i = 0;
    for(;i < pool->thread_num;++i){
       pthread_join(pool->pthreads[i],NULL);
    }
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->queue_empty);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_not_full);
    free(pool->pthreads);
    
    struct job* pjob = NULL;
    while(pool->head){
        pjob = pool->head->next;
        free(pool->head);
        pool->head = pjob;
    }
    free(pool);
    return 0;
}
void*              threadpool_function(void* arg){
     struct ThreadPool* pool = (struct ThreadPool*)arg;
     struct job* pjob = NULL;

     while(1){
        pthread_mutex_lock(&pool->mutex);
        while((pool->queue_cur_num == 0) && !pool->pool_close){
            pthread_cond_wait(&pool->queue_not_empty,&pool->mutex);
        }
        if(pool->pool_close){
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }


        pool->queue_cur_num--;
        pjob = pool->head;
        if(pool->queue_cur_num == 0){
            //如果只有一个任务的话
            pool->head = pool->tail = NULL;
        }else{
            pool->head = pjob->next;
        }

        if(pool->queue_cur_num == 0){
            pthread_cond_signal(&pool->queue_empty);
        }

        if(pool->queue_cur_num == pool->queue_max_num - 1){
            pthread_cond_signal(&pool->queue_not_full);
        }
        pthread_mutex_unlock(&pool->mutex);
        (*(pjob->callback))(pjob->arg);
        free(pjob);
        pjob = NULL;
     }
}
