#include "server.h"
#include "command.h"
#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RedisServer *createServer(void) {
  RedisServer *server = calloc(1, sizeof(RedisServer));
  if (!server)
    return NULL;

  // Network defaults
  server->port = 6379;
  server->bindaddr = strdup("127.0.0.1");
  server->tcp_backlog = 511;
  server->clients_count = 0;

  // Initialize storage
  server->db = createStore();
  if (!server->db) {
    freeServer(server);
    return NULL;
  }

  // Initialize statistics
  server->total_commands_processed = 0;
  server->keyspace_hits = 0;
  server->keyspace_misses = 0;

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
  if (!server)
    return;

  if (server->bindaddr) {
    free(server->bindaddr);
  }

  if (server->fd > 0) {
    close(server->fd);
  }

  if (server->db) {
    freeStore(server->db);
  }

  if (server->commands) {
    freeCommandTable(server->commands);
  }

  free(server);
}

void serverCron(RedisServer *server) {
  // Clean up expired keys
  clearExpired(server->db);

  // Future implementations:
  // - Update statistics
  // - Check replication status
  // - Memory usage monitoring
}
