#include "server.h"
#include "config.h"
#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RedisServer *createServer(ServerConfig *config) {
  RedisServer *server = calloc(1, sizeof(RedisServer));
  if (!server)
    return NULL;

  // Network defaults
  server->port = config->port;
  server->bindaddr = config->bindaddr;

  server->dir = config->dir;
  server->filename = config->dbfilename;

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

  if (server->dir) {
    free(server->dir);
  }

  if (server->filename) {
    free(server->filename);
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
