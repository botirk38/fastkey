#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include <stdint.h>

typedef struct redisServer {
  int fd;            // Main server socket file descriptor
  uint16_t port;     // Server listening port
  char *bindaddr;    // Binding address
  int clients_count; // Connected clients count

  // Configuration
  int tcp_backlog; // TCP listen backlog

  // Database
  struct redisDb *db; // Redis database

  // Persistence
  char *rdb_filename; // RDB file name

  // Replication
  int repl_enabled;       // Replication enabled flag
  char *repl_master_host; // Master host for replication
  int repl_master_port;   // Master port for replication
} redisServer;

// Server initialization and cleanup
redisServer *createServer(void);
void freeServer(redisServer *server);

// Server operations
int initServer(redisServer *server);
void serverCron(redisServer *server);

#endif
