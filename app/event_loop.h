#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "resp.h"
#include "server.h"
#include <stdlib.h>
#include <sys/epoll.h>

#define MAX_EVENTS 1024
#define MAX_CLIENTS 1024

typedef struct {
  int fd;
  RespBuffer *buffer;
} ClientState;

typedef struct {
  int epollFd;
  struct epoll_event *events;
  int running;
  ClientState *clients[MAX_EVENTS];
  size_t clientCount;

} EventLoop;

EventLoop *createEventLoop(void);
void eventLoopAddFd(EventLoop *loop, int fd, int events);
void eventLoopRemoveFd(EventLoop *loop, int fd);
void eventLoopStart(EventLoop *loop, RedisServer *server);

#endif
