#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#define MAX_CLIENTS 1024
#define NUM_THREADS 10

#include <pthread.h>


typedef struct {
  int start;
  int end;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int client_fds[MAX_CLIENTS];

} ClientQueue;

static pthread_t workers[NUM_THREADS];



void enqueue_client(int client_fd);
int dequeue_client();

#endif // !THREAD_POOL_H
