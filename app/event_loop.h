#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "command_queue.h"
#include "resp.h"
#include "server.h"
#include <stdlib.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#endif

#define MAX_EVENTS 1024
#define MAX_CLIENTS 1024

typedef struct {
  int fd;
  RespBuffer *buffer;
  int in_transaction;
  CommandQueue *queue;
} ClientState;

typedef struct {
#ifdef __linux__
  int epollFd;
  struct epoll_event *events;
#elif defined(__APPLE__)
  int kqueueFd;
  struct kevent *events;
#endif
  int running;
  ClientState *clients[MAX_EVENTS];
  size_t clientCount;
} EventLoop;

EventLoop *createEventLoop(void);
void eventLoopAddFd(EventLoop *loop, int fd, int events);
void eventLoopRemoveFd(EventLoop *loop, int fd);
void eventLoopStart(EventLoop *loop, RedisServer *server);

#endif

