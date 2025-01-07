#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "server.h"
#include <sys/epoll.h>

#define MAX_EVENTS 1024

typedef struct {
  int epoll_fd;
  struct epoll_event *events;
  int running;
} event_loop;

event_loop *create_event_loop(void);
void event_loop_add_fd(event_loop *loop, int fd, int events);
void event_loop_remove_fd(event_loop *loop, int fd);
void event_loop_start(event_loop *loop, redisServer *server);

#endif
