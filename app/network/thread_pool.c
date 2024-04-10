#include "thread_pool.h"
#include <stdio.h>
#include <unistd.h>


ClientQueue queue = {
    .start = 0,
    .end = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};




void enqueue_client(int client_fd) {
    pthread_mutex_lock(&queue.mutex);

    int next_end = (queue.end + 1) % MAX_CLIENTS;
    if(next_end == queue.start) { // Queue is full
        printf("Connection queue is full. Dropping client.\n");
        close(client_fd);
    } else {
        queue.client_fds[queue.end] = client_fd;
        queue.end = next_end;
        pthread_cond_signal(&queue.cond);
    }

    pthread_mutex_unlock(&queue.mutex);
}


int dequeue_client() {
  pthread_mutex_lock(&queue.mutex);
  while (queue.start == queue.end) {
    pthread_cond_wait(&queue.cond, &queue.mutex);
  }
  int client_fd = queue.client_fds[queue.start];
  queue.start = (queue.start + 1) % MAX_CLIENTS;
  pthread_mutex_unlock(&queue.mutex);
  return client_fd;
}


