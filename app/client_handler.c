#include "client_handler.h"
#include "command.h"
#include "networking.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void handleNewClient(EventLoop *loop, RedisServer *server, int clientFd) {
  if (clientFd >= MAX_CLIENTS) {
    close(clientFd);
    return;
  }

  ClientState *client = malloc(sizeof(ClientState));
  client->fd = clientFd;
  client->buffer = createRespBuffer();

  loop->clients[clientFd] = client;
  loop->clientCount++;

  eventLoopAddFd(loop, clientFd, EPOLLIN);
}

void handleClientCommand(RedisServer *server, int fd, RespValue *command) {
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    return;
  }

  const char *response = executeCommand(server->db, command);

  sendReply(server, fd, response);
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
    handleClientCommand(server, fd, command);
    freeRespValue(command);
  }
}
