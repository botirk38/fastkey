#include "client_handler.h"
#include "command.h"
#include "command_queue.h"
#include "logger.h"
#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ClientState *handleNewClient(RedisServer *server, int clientFd) {
  LOG_DEBUG("Attempting to create new client connection for fd %d", clientFd);

  if (clientFd >= MAX_CLIENTS) {
    LOG_WARN("Client connection rejected - exceeded MAX_CLIENTS limit (fd: %d, "
             "max: %d)",
             clientFd, MAX_CLIENTS);
    close(clientFd);
    return NULL;
  }

  ClientState *client = malloc(sizeof(ClientState));
  if (!client) {
    LOG_ERROR("Failed to allocate memory for new client state (fd: %d)",
              clientFd);
    close(clientFd);
    return NULL;
  }

  client->fd = clientFd;
  client->buffer = createRespBuffer();
  if (!client->buffer) {
    LOG_ERROR("Failed to create RESP buffer for client (fd: %d)", clientFd);
    free(client);
    close(clientFd);
    return NULL;
  }
  client->in_transaction = 0;
  client->queue = createCommandQueue();
  if (!client->queue) {
    LOG_ERROR("Failed to create command queue for client (fd: %d)", clientFd);
    freeRespBuffer(client->buffer);
    free(client);
    close(clientFd);
    return NULL;
  }

  LOG_INFO("New client connected successfully (fd: %d, transaction: %d)",
           clientFd, client->in_transaction);
  return client;
}

void handleClientCommand(RedisServer *server, int fd, RespValue *command,
                         ClientState *clientState) {
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    LOG_DEBUG(
        "Received invalid command from client (fd: %d, type: %d, len: %d)", fd,
        command->type, command->data.array.len);
    return;
  }

  LOG_DEBUG("Processing command from client (fd: %d, in_transaction: %d)", fd,
            clientState->in_transaction);

  const char *response =
      executeCommand(server, server->db, command, clientState);

  if (response) {
    LOG_TRACE("Sending response to client (fd: %d, response: %s)", fd,
              response);
    sendReply(server, fd, response);
  } else {
    LOG_DEBUG("No response generated for client command (fd: %d)", fd);
  }
}

void handleClientData(RedisServer *server, ClientState *client) {
  char readBuf[1024];
  LOG_DEBUG("Starting client data handling loop (fd: %d)", client->fd);

  while (1) {
    ssize_t n = recv(client->fd, readBuf, sizeof(readBuf), 0);

    if (n == 0) {
      LOG_INFO("Client disconnected (fd: %d)", client->fd);
      break;
    }

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue; // Non-blocking operation, try again
      }
      LOG_ERROR("Read error on client socket (fd: %d): %s", client->fd,
                strerror(errno));
      break;
    }
    LOG_TRACE("Received data from client (fd: %d, bytes: %zd)", client->fd, n);
    appendRespBuffer(client->buffer, readBuf, n);

    RespValue *command;
    while (parseResp(client->buffer, &command) == RESP_OK) {
      LOG_TRACE("Successfully parsed RESP command (fd: %d)", client->fd);
      handleClientCommand(server, client->fd, command, client);
      freeRespValue(command);
    }
  }
}

void freeClientState(ClientState *client) {
  if (!client) {
    return;
  }
  if (client->buffer) {
    freeRespBuffer(client->buffer);
  }
  if (client->queue) {
    freeCommandQueue(client->queue);
  }
  close(client->fd);
  free(client);
}

void handleClientConnection(void *arg) {
  ClientHandlerArg *client_arg = (ClientHandlerArg *)arg;
  LOG_DEBUG("Initializing client handler thread (fd: %d, server_port: %d)",
            client_arg->clientFd, client_arg->server->port);

  ClientState *client =
      handleNewClient(client_arg->server, client_arg->clientFd);

  if (client) {
    LOG_INFO("Starting client data handling (fd: %d)", client_arg->clientFd);
    handleClientData(client_arg->server, client);
    freeClientState(client);
  } else {
    LOG_WARN("Failed to initialize client state (fd: %d)",
             client_arg->clientFd);
  }

  LOG_DEBUG("Cleaning up client handler thread (fd: %d)", client_arg->clientFd);
  LOG_INFO("Client handler thread terminated (fd: %d)", client_arg->clientFd);
  free(client_arg);
}
