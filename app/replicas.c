#include "replicas.h"
#include "replication.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void initReplicaList(RedisServer *server) {
  server->repl_info->replicas = malloc(sizeof(Replicas));
  server->repl_info->replicas->replica_capacity = INITIAL_REPLICA_CAPACITY;
  server->repl_info->replicas->replica_fds = (int *)malloc(
      sizeof(int) * server->repl_info->replicas->replica_capacity);
  server->repl_info->replicas->replica_count = 0;
}

void addReplica(RedisServer *server, int fd) {
  if (server->repl_info->replicas->replica_count >=
      server->repl_info->replicas->replica_capacity) {
    server->repl_info->replicas->replica_capacity *= 2;
    server->repl_info->replicas->replica_fds = (int *)realloc(
        server->repl_info->replicas->replica_fds,
        sizeof(int) * server->repl_info->replicas->replica_capacity);
  }
  server->repl_info->replicas
      ->replica_fds[server->repl_info->replicas->replica_count++] = fd;
}

void removeReplica(RedisServer *server, int fd) {
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    if (server->repl_info->replicas->replica_fds[i] == fd) {
      memmove(&server->repl_info->replicas->replica_fds[i],
              &server->repl_info->replicas->replica_fds[i + 1],
              sizeof(int) *
                  (server->repl_info->replicas->replica_count - i - 1));
      server->repl_info->replicas->replica_count--;
      break;
    }
  }
}

void propagateCommand(RedisServer *server, RespValue *command) {
  // Convert command to RESP format
  char *cmd_str = createRespArrayFromElements(command->data.array.elements,
                                              command->data.array.len);

  // Send to all replicas
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    int fd = server->repl_info->replicas->replica_fds[i];
    ssize_t written = write(fd, cmd_str, strlen(cmd_str));
    if (written < 0) {
      // Handle write error - remove failed replica
      removeReplica(server, fd);
      close(fd);
      i--; // Adjust index since we removed an element
    }
  }

  free(cmd_str);
}

void freeReplicas(RedisServer *server) {
  // Close all replica connections
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    close(server->repl_info->replicas->replica_fds[i]);
  }

  // Free the array of file descriptors
  free(server->repl_info->replicas->replica_fds);

  // Free the replicas struct itself
  free(server->repl_info->replicas);
  server->repl_info->replicas = NULL;
}
