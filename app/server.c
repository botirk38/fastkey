#include "server.h"
#include "config.h"
#include "networking.h"
#include "replication.h"
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

  server->repl_info = malloc(sizeof(ReplicationInfo));
  server->repl_info->master_info = NULL;
  server->repl_info->replication_id =
      strdup("8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb");
  server->repl_info->repl_offset = 0;

  if (config->is_replica) {
    server->repl_info->master_info = malloc(sizeof(MasterInfo));
    server->repl_info->master_info->host = strdup(config->master_host);
    server->repl_info->master_info->port = config->master_port;
  }
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

  if (server->repl_info->master_info) {
    if (startReplication(server->repl_info->master_info, server->port) != 0) {
      return 1;
    }
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

  if (server->repl_info) {
    free(server->repl_info->master_info->host);
    free(server->repl_info->master_info);
    free(server->repl_info);
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
