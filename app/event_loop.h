#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "command_queue.h"
#include "resp.h"
#include "server.h"
#include <stdlib.h>
#include <sys/epoll.h>

#define MAX_EVENTS 1024
#define MAX_CLIENTS 1024

typedef struct {
  int fd;
  RespBuffer *buffer;
  int in_transaction;
  CommandQueue *queue;

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
