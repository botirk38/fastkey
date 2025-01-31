#include "client_handler.h"
#include "command.h"
#include "command_queue.h"
#include "event_loop.h"
#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#define EVENT_READ EPOLLIN
#elif defined(__APPLE__)
#include <sys/event.h>
#define EVENT_READ EVFILT_READ
#endif

void handleNewClient(EventLoop *loop, RedisServer *server, int clientFd) {
  if (clientFd >= MAX_CLIENTS) {
    close(clientFd);
    return;
  }

  ClientState *client = malloc(sizeof(ClientState));
  client->fd = clientFd;
  client->buffer = createRespBuffer();
  client->in_transaction = 0;
  client->queue = createCommandQueue();

  loop->clients[clientFd] = client;
  loop->clientCount++;

  eventLoopAddFd(loop, clientFd, EVENT_READ);
}

void handleClientCommand(RedisServer *server, int fd, RespValue *command,
                         ClientState *clientState) {
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    return;
  }

  const char *response =
      executeCommand(server, server->db, command, clientState);

  if (response) {

    sendReply(server, fd, response);
  }
}

void handleClientData(EventLoop *loop, RedisServer *server, int fd) {
  ClientState *client = loop->clients[fd];
  char readBuf[1024];

  ssize_t n = read(fd, readBuf, sizeof(readBuf));
  if (n <= 0) {
    eventLoopRemoveFd(loop, fd);
    freeRespBuffer(client->buffer);
    free(client);
    loop->clients[fd] = NULL;
    loop->clientCount--;
    closeClientConnection(server, fd);
    return;
  }

  appendRespBuffer(client->buffer, readBuf, n);

  RespValue *command;
  while (parseResp(client->buffer, &command) == RESP_OK) {
    handleClientCommand(server, fd, command, client);
    freeRespValue(command);
  }
}
