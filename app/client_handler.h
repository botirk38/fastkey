#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "command_queue.h"

#include "resp.h"
#include "server.h"

#define MAX_CLIENTS 1024

typedef struct {
  int fd;
  RespBuffer *buffer;
  int in_transaction;
  CommandQueue *queue;
} ClientState;

typedef struct {
  RedisServer *server;
  int clientFd;
} ClientHandlerArg;

ClientState *handleNewClient(RedisServer *server, int clientFd);
void handleClientCommand(RedisServer *server, int fd, RespValue *command,
                         ClientState *clientState);
void handleClientData(RedisServer *server, ClientState *client);
void handleClientConnection(void *arg);

#endif
