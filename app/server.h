#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include "redis_store.h"

typedef struct {
  char *master_host;
  int master_port;
} ReplicationInfo;

typedef struct RedisServer {
  // Networking
  int fd;            // Main server socket file descriptor
  char *bindaddr;    // Binding address
  int port;          // TCP listening port
  int tcp_backlog;   // TCP listen() backlog
  int clients_count; // Connected clients counter

  // Data Storage
  RedisStore *db; // Main key-value storage

  // RDB File
  char *dir;
  char *filename;

  // Replication Info

  ReplicationInfo *repl_info;

  // Server Statistics
  long long total_commands_processed;
  long long keyspace_hits;
  long long keyspace_misses;
} RedisServer;

/**
 * Creates a new Redis server instance with default configuration.
 *
 * @return Pointer to new RedisServer instance, or NULL on failure
 */
RedisServer *createServer(ServerConfig *config);

/**
 * Initializes server components including networking and storage.
 *
 * @param server Pointer to RedisServer instance
 * @return 0 on success, non-zero on failure
 */
int initServer(RedisServer *server);

/**
 * Cleans up and frees all server resources.
 *
 * @param server Pointer to RedisServer instance
 */
void freeServer(RedisServer *server);

/**
 * Periodic server tasks handler.
 * Handles tasks like:
 * - Expired keys cleanup
 * - Statistics updates
 * - Health checks
 *
 * @param server Pointer to RedisServer instance
 */
void serverCron(RedisServer *server);

#endif
