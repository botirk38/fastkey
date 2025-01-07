#include "event_loop.h"
#include "networking.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

event_loop *create_event_loop(void) {
  event_loop *loop = malloc(sizeof(event_loop));
  loop->epoll_fd = epoll_create1(0);
  loop->events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
  loop->running = 1;
  return loop;
}

void event_loop_add_fd(event_loop *loop, int fd, int events) {
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;
  epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void event_loop_remove_fd(event_loop *loop, int fd) {
  epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

void event_loop_start(event_loop *loop, redisServer *server) {
  // Add server socket to event loop
  event_loop_add_fd(loop, server->fd, EPOLLIN);

  while (loop->running) {
    int nfds = epoll_wait(loop->epoll_fd, loop->events, MAX_EVENTS, -1);

    for (int i = 0; i < nfds; i++) {
      int fd = loop->events[i].data.fd;

      if (fd == server->fd) {
        // New connection
        int client_fd = acceptClient(server);
        if (client_fd >= 0) {
          event_loop_add_fd(loop, client_fd, EPOLLIN);
        }
      } else {
        // Client socket ready for reading
        char buffer[1024];
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n <= 0) {
          // Client disconnected or error
          event_loop_remove_fd(loop, fd);
          closeClientConnection(server, fd);
        } else {
          // Send PONG response
          sendReply(server, fd, "+PONG\r\n");
        }
      }
    }
  }
}
