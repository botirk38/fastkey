#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

typedef struct {
    void (*task)(void *arg);
    void *arg;
} ThreadTask;

typedef struct {
    pthread_t *threads;
    ThreadTask *tasks;
    int task_capacity;
    int task_count;
    int thread_count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int running;
} ThreadPool;

ThreadPool *createThreadPool(int num_threads);
int threadPoolAdd(ThreadPool *pool, void (*task)(void *), void *arg);
void threadPoolDestroy(ThreadPool *pool);

#endif

