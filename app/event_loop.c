#include "event_loop.h"
#include "client_handler.h"
#include "networking.h"
#include <stdlib.h>

#ifdef __linux__
static int create_event_loop_fd(void) { return epoll_create1(0); }

static void *create_events_array(void) {
  return malloc(sizeof(struct epoll_event) * MAX_EVENTS);
}

static void add_fd(EventLoop *loop, int fd) {
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  epoll_ctl(loop->epollFd, EPOLL_CTL_ADD, fd, &ev);
}

static void remove_fd(EventLoop *loop, int fd) {
  epoll_ctl(loop->epollFd, EPOLL_CTL_DEL, fd, NULL);
}

static int wait_for_events(EventLoop *loop) {
  return epoll_wait(loop->epollFd, loop->events, MAX_EVENTS, 0);
}

static int get_fd_from_event(EventLoop *loop, int index) {
  return loop->events[index].data.fd;
}
#elif defined(__APPLE__)
static int create_event_loop_fd(void) { return kqueue(); }

static void *create_events_array(void) {
  return malloc(sizeof(struct kevent) * MAX_EVENTS);
}

static void add_fd(EventLoop *loop, int fd) {
  struct kevent ev;
  EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
  kevent(loop->kqueueFd, &ev, 1, NULL, 0, NULL);
}

static void remove_fd(EventLoop *loop, int fd) {
  struct kevent ev;
  EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  kevent(loop->kqueueFd, &ev, 1, NULL, 0, NULL);
}

static int wait_for_events(EventLoop *loop) {
  return kevent(loop->kqueueFd, NULL, 0, loop->events, MAX_EVENTS, NULL);
}

static int get_fd_from_event(EventLoop *loop, int index) {
  return (int)loop->events[index].ident;
}
#endif

EventLoop *createEventLoop(void) {
  EventLoop *loop = malloc(sizeof(EventLoop));

#ifdef __linux__
  loop->epollFd = create_event_loop_fd();
#elif defined(__APPLE__)
  loop->kqueueFd = create_event_loop_fd();
#endif

  loop->events = create_events_array();
  loop->running = 1;
  loop->clientCount = 0;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    loop->clients[i] = NULL;
  }
  return loop;
}

void eventLoopAddFd(EventLoop *loop, int fd, int events) { add_fd(loop, fd); }

void eventLoopRemoveFd(EventLoop *loop, int fd) { remove_fd(loop, fd); }

void eventLoopStart(EventLoop *loop, RedisServer *server) {
  add_fd(loop, server->fd);

  while (loop->running) {
    int nfds = wait_for_events(loop);

    for (int i = 0; i < nfds; i++) {
      int fd = get_fd_from_event(loop, i);

      if (fd == server->fd) {
        int clientFd = acceptClient(server);
        if (clientFd >= 0) {
          handleNewClient(loop, server, clientFd);
        }
      } else {
        handleClientData(loop, server, fd);
      }
    }
  }
}
