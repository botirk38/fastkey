#include "server.h"
#include "command.h"
#include "config.h"
#include "networking.h"
#include "replicas.h"
#include "resp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;

void *handleReplicationThread(void *args) {
  ReplicationArgs *repl_args = (ReplicationArgs *)args;
  RedisServer *server = repl_args->server;
  int fd = repl_args->fd;

  handleReplicationCommands(server, fd);
  free(repl_args);
  return NULL;
}

static bool isValidCommand(RespValue *command) {
  return command->type == RespTypeArray && command->data.array.len > 0 &&
         command->data.array.elements[0] &&
         command->data.array.elements[0]->type == RespTypeString;
}

static bool isReplconfGetack(RespValue *command) {
  return strcasecmp(command->data.array.elements[0]->data.string.str,
                    "REPLCONF") == 0 &&
         command->data.array.len > 1 &&
         strcasecmp(command->data.array.elements[1]->data.string.str,
                    "GETACK") == 0;
}

static void processCommand(RedisServer *server, RespValue *command, int fd) {
  printf("[Replication] Processing command: %s\n",
         command->data.array.elements[0]->data.string.str);

  ClientState clientState = {0};
  const char *response =
      executeCommand(server, server->db, command, &clientState);

  if (response) {
    if (isReplconfGetack(command)) {
      send(fd, response, strlen(response), 0);
    }
    free((void *)response);
  }
}

int handleReplicationCommands(RedisServer *server, int fd) {
  printf("[Replication] Starting command processing\n");
  RespBuffer *resp_buffer = createRespBuffer();
  if (!resp_buffer) {
    printf("[Error] Failed to create RESP buffer\n");
    return -1;
  }

  char read_buffer[4096];
  ssize_t n = 0;
  size_t bytes_processed = 0;

  while ((n = recv(fd, read_buffer, sizeof(read_buffer), 0)) > 0) {
    read_buffer[n] = '\0';
    printf("[Replication] Received %zd bytes\n", n);

    if (appendRespBuffer(resp_buffer, read_buffer, n) != RESP_OK) {
      printf("[Error] Failed to append to RESP buffer\n");
      freeRespBuffer(resp_buffer);
      return -1;
    }

    while (1) {
      RespValue *command = NULL;
      int parse_result = parseResp(resp_buffer, &command);

      if (parse_result == RESP_INCOMPLETE) {
        break;
      }

      if (parse_result == RESP_OK && command) {
        if (isValidCommand(command)) {

          size_t cmd_bytes = calculateCommandSize(command);

          printf("Command Bytes: %lu\n", cmd_bytes);

          processCommand(server, command, fd);

          printf("Previous Repl offset: %llu\n",
                 server->repl_info->repl_offset);

          server->repl_info->repl_offset += cmd_bytes;

          printf(" New Repl offset: %llu\n", server->repl_info->repl_offset);
        }
        freeRespValue(command);
      }
    }
  }

  freeRespBuffer(resp_buffer);
  return 0;
}

RedisServer *createServer(ServerConfig *config) {
  RedisServer *server = calloc(1, sizeof(RedisServer));
  if (!server)
    return NULL;

  // Network defaults
  server->port = config->port;
  server->bindaddr = config->bindaddr;
  server->dir = config->dir;
  server->filename = config->dbfilename;

  // Initialize replication info
  server->repl_info = malloc(sizeof(ReplicationInfo));
  if (!server->repl_info) {
    freeServer(server);
    return NULL;
  }
  server->repl_info->master_info = NULL;
  server->repl_info->replicas = NULL;
  server->repl_info->replication_id =
      strdup("8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb");
  if (!server->repl_info->replication_id) {
    freeServer(server);
    return NULL;
  }
  server->repl_info->repl_offset = 0;

  if (config->is_replica) {
    server->repl_info->master_info = malloc(sizeof(MasterInfo));
    if (!server->repl_info->master_info) {
      freeServer(server);
      return NULL;
    }
    server->repl_info->master_info->host = strdup(config->master_host);
    if (!server->repl_info->master_info->host) {
      freeServer(server);
      return NULL;
    }
    server->repl_info->master_info->port = config->master_port;
  } else {
    initReplicaList(server);
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
  if (initServerSocket(server) != 0) {
    fprintf(stderr, "Failed to initialize server socket\n");
    return 1;
  }

  // Handle replication if we're a replica
  if (server->repl_info->master_info) {
    if (startReplication(server->repl_info->master_info, server->port) != 0) {
      return 1;
    }

    // Initialize replication thread
    ReplicationArgs *args = malloc(sizeof(ReplicationArgs));
    args->server = server;
    args->fd = server->repl_info->master_info->fd;

    pthread_t replication_thread;
    if (pthread_create(&replication_thread, NULL, handleReplicationThread,
                       args) != 0) {
      printf("Failed to create replication thread\n");
      free(args);
      return 1;
    }

    pthread_detach(replication_thread);
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
    if (server->repl_info->master_info) {
      free(server->repl_info->master_info->host);
      free(server->repl_info->master_info);
    } else {
      freeReplicas(server);
    }
    free(server->repl_info->replication_id);
    free(server->repl_info);
  }

  free(server);
}

void serverCron(RedisServer *server) { clearExpired(server->db); }
