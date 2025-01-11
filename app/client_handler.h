#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "event_loop.h"
#include "resp.h"
#include "server.h"

void handleNewClient(EventLoop *loop, RedisServer *server, int clientFd);
void handleClientCommand(RedisServer *server, int fd, RespValue *command,
                         ClientState *ClientState);
void handleClientData(EventLoop *loop, RedisServer *server, int fd);

#endif
