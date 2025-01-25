#include "server.h"
#include "command.h"
#include "config.h"
#include "networking.h"
#include "replicas.h"
#include "replication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *handlePropagatedCommandsThread(void *args) {
  PropagationArgs *prop_args = (PropagationArgs *)args;
  RedisServer *server = prop_args->server;
  int fd = prop_args->fd;

  handlePropagatedCommands(server, fd);

  free(prop_args);
  return NULL;
}

int handlePropagatedCommands(RedisServer *server, int fd) {
  printf("[Replication] Starting command propagation processing\n");

  RespBuffer *resp_buffer = createRespBuffer();
  if (!resp_buffer) {
    printf("[Error] Failed to create RESP buffer\n");
    return -1;
  }

  char read_buffer[4096];
  ssize_t n = 0;

  while ((n = recv(fd, read_buffer, sizeof(read_buffer), 0)) > 0) {
    read_buffer[n] = '\0'; // Null terminate for safe printing
    printf("[Replication] Received %zd bytes from master\n", n);
    printf("[Replication] Raw buffer contents:\n%s\n", read_buffer);

    if (appendRespBuffer(resp_buffer, read_buffer, n) != RESP_OK) {
      printf("[Error] Failed to append to RESP buffer\n");
      freeRespBuffer(resp_buffer);
      return -1;
    }

    printf("[Replication] Current RESP buffer contents (%zu bytes):\n%s\n",
           resp_buffer->used, resp_buffer->buffer);

    while (1) {
      RespValue *command = NULL;
      int parse_result = parseResp(resp_buffer, &command);

      if (parse_result == RESP_INCOMPLETE) {
        printf("[Replication] Incomplete command, waiting for more data\n");
        printf("[Replication] Remaining buffer (%zu bytes):\n%s\n",
               resp_buffer->used, resp_buffer->buffer);
        break;
      }

      if (parse_result == RESP_OK && command) {
        if (command->type == RespTypeArray && command->data.array.len > 0 &&
            command->data.array.elements[0] &&
            command->data.array.elements[0]->type == RespTypeString) {

          printf("[Replication] Processing command: %s with %lu arguments\n",
                 command->data.array.elements[0]->data.string.str,
                 command->data.array.len - 1);

          // Log command arguments
          for (int i = 1; i < command->data.array.len; i++) {
            if (command->data.array.elements[i]->type == RespTypeString) {
              printf("[Replication] Arg %d: %s\n", i,
                     command->data.array.elements[i]->data.string.str);
            }
          }

          ClientState clientState = {0};
          const char *response =
              executeCommand(server, server->db, command, &clientState);

          if (response) {
            printf("[Replication] Command executed (response discarded)\n");
            free((void *)response);
          }
        }
        freeRespValue(command);
      } else if (parse_result != RESP_INCOMPLETE) {
        printf("[Error] Failed to parse command (error: %d)\n", parse_result);
      }
    }
  }

  if (n == 0) {
    printf("[Replication] Master closed connection\n");
  } else if (n < 0) {
    printf("[Error] Read error from master: %s\n", strerror(errno));
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

  server->repl_info = malloc(sizeof(ReplicationInfo));
  server->repl_info->master_info = NULL;
  server->repl_info->replicas = NULL;
  server->repl_info->replication_id =
      strdup("8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb");
  server->repl_info->repl_offset = 0;

  if (config->is_replica) {
    server->repl_info->master_info = malloc(sizeof(MasterInfo));
    server->repl_info->master_info->host = strdup(config->master_host);
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
  // Initialize networking
  if (initServerSocket(server) != 0) {
    fprintf(stderr, "Failed to initialize server socket\n");
    return 1;
  }

  if (server->repl_info->master_info) {
    if (startReplication(server->repl_info->master_info, server->port) != 0) {
      return 1;
    }

    // Create thread arguments
    PropagationArgs *args = malloc(sizeof(PropagationArgs));
    args->server = server;
    args->fd = server->repl_info->master_info->fd;

    // Create thread
    pthread_t propagation_thread;
    if (pthread_create(&propagation_thread, NULL,
                       handlePropagatedCommandsThread, args) != 0) {
      printf("Failed to create propagation thread\n");
      free(args);
      return 1;
    }

    // Detach thread to allow it to clean up automatically
    pthread_detach(propagation_thread);
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
