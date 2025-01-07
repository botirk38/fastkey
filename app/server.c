#include "server.h"
#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RedisServer *createServer(void) {
  RedisServer *server = calloc(1, sizeof(RedisServer));
  if (!server)
    return NULL;

  // Set default values
  server->port = 6379;
  server->bindaddr = strdup("127.0.0.1");
  server->tcp_backlog = 511;
  server->clients_count = 0;

  return server;
}

int initServer(RedisServer *server) {
  // Initialize networking
  if (initServerSocket(server) != 0) {
    fprintf(stderr, "Failed to initialize server socket\n");
    return 1;
  }

  return 0;
}

void freeServer(RedisServer *server) {
  if (server) {
    if (server->bindaddr) {
      free(server->bindaddr);
    }
    if (server->fd > 0) {
      close(server->fd);
    }
    free(server);
  }
}

void serverCron(RedisServer *server) {
  // This will be used for periodic tasks like:
  // - Cleaning up expired keys
  // - Statistics gathering
  // - Replication health checks
  // For now, it's just a placeholder
}
