#include "event_loop.h"
#include "client_handler.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include "networking.h"

EventLoop* createEventLoop(void) {
    EventLoop* loop = malloc(sizeof(EventLoop));
    loop->epollFd = epoll_create1(0);
    loop->events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    loop->running = 1;
    loop->clientCount = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        loop->clients[i] = NULL;
    }
    return loop;
}

void eventLoopAddFd(EventLoop* loop, int fd, int events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(loop->epollFd, EPOLL_CTL_ADD, fd, &ev);
}

void eventLoopRemoveFd(EventLoop* loop, int fd) {
    epoll_ctl(loop->epollFd, EPOLL_CTL_DEL, fd, NULL);
}

void eventLoopStart(EventLoop* loop, RedisServer* server) {
    eventLoopAddFd(loop, server->fd, EPOLLIN);

    while (loop->running) {
        int nfds = epoll_wait(loop->epollFd, loop->events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; i++) {
            int fd = loop->events[i].data.fd;
            
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

