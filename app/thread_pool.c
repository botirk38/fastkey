#include "thread_pool.h"
#include <stdlib.h>
#include <unistd.h>

static void *worker(void *arg) {
  ThreadPool *pool = (ThreadPool *)arg;

  while (pool->running) {
    pthread_mutex_lock(&pool->lock);

    while (pool->task_count == 0 && pool->running) {
      pthread_cond_wait(&pool->not_empty, &pool->lock);
    }

    if (!pool->running) {
      pthread_mutex_unlock(&pool->lock);
      break;
    }

    ThreadTask task = pool->tasks[--pool->task_count];
    pthread_cond_signal(&pool->not_full);
    pthread_mutex_unlock(&pool->lock);

    task.task(task.arg);
  }

  return NULL;
}

ThreadPool *createThreadPool(int num_threads) {
  ThreadPool *pool = malloc(sizeof(ThreadPool));
  pool->thread_count = num_threads;
  pool->task_capacity = num_threads * 2;
  pool->task_count = 0;
  pool->running = 1;

  pool->threads = malloc(sizeof(pthread_t) * num_threads);
  pool->tasks = malloc(sizeof(ThreadTask) * pool->task_capacity);

  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->not_empty, NULL);
  pthread_cond_init(&pool->not_full, NULL);

  for (int i = 0; i < num_threads; i++) {
    pthread_create(&pool->threads[i], NULL, worker, pool);
  }

  return pool;
}

void threadPoolAdd(ThreadPool *pool, void (*task)(void *), void *arg) {
  pthread_mutex_lock(&pool->lock);

  while (pool->task_count == pool->task_capacity) {
    pthread_cond_wait(&pool->not_full, &pool->lock);
  }

  ThreadTask thread_task = {task, arg};
  pool->tasks[pool->task_count++] = thread_task;

  pthread_cond_signal(&pool->not_empty);
  pthread_mutex_unlock(&pool->lock);
}

void threadPoolDestroy(ThreadPool *pool) {
  pthread_mutex_lock(&pool->lock);
  pool->running = 0;
  pthread_cond_broadcast(&pool->not_empty);
  pthread_mutex_unlock(&pool->lock);

  for (int i = 0; i < pool->thread_count; i++) {
    pthread_join(pool->threads[i], NULL);
  }

  pthread_mutex_destroy(&pool->lock);
  pthread_cond_destroy(&pool->not_empty);
  pthread_cond_destroy(&pool->not_full);

  free(pool->threads);
  free(pool->tasks);
  free(pool);
}

