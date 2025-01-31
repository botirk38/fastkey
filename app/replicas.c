#include "replicas.h"
#include "handshake.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void initReplicaList(RedisServer *server) {
  server->repl_info->replicas = malloc(sizeof(Replicas));
  server->repl_info->replicas->replica_capacity = INITIAL_REPLICA_CAPACITY;
  server->repl_info->replicas->replicas =
      malloc(sizeof(Replica) * server->repl_info->replicas->replica_capacity);
  server->repl_info->replicas->replica_count = 0;
}

void addReplica(RedisServer *server, int fd) {
  if (server->repl_info->replicas->replica_count >=
      server->repl_info->replicas->replica_capacity) {
    server->repl_info->replicas->replica_capacity *= 2;
    server->repl_info->replicas->replicas = realloc(
        server->repl_info->replicas->replicas,
        sizeof(Replica) * server->repl_info->replicas->replica_capacity);
  }

  Replica new_replica = {.fd = fd, .ack_offset = 0};

  server->repl_info->replicas
      ->replicas[server->repl_info->replicas->replica_count++] = new_replica;
}

void removeReplica(RedisServer *server, int fd) {
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    if (server->repl_info->replicas->replicas[i].fd == fd) {
      memmove(&server->repl_info->replicas->replicas[i],
              &server->repl_info->replicas->replicas[i + 1],
              sizeof(Replica) *
                  (server->repl_info->replicas->replica_count - i - 1));
      server->repl_info->replicas->replica_count--;
      break;
    }
  }
}

void propagateCommand(RedisServer *server, RespValue *command) {
  char *cmd_str = createRespArrayFromElements(command->data.array.elements,
                                              command->data.array.len);

  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    int fd = server->repl_info->replicas->replicas[i].fd;
    ssize_t written = write(fd, cmd_str, strlen(cmd_str));

    if (written < 0) {
      removeReplica(server, fd);
      close(fd);
      i--;
    }

    server->repl_info->repl_offset += written;
  }

  free(cmd_str);
}

void freeReplicas(RedisServer *server) {
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    close(server->repl_info->replicas->replicas[i].fd);
  }

  free(server->repl_info->replicas->replicas);
  free(server->repl_info->replicas);
  server->repl_info->replicas = NULL;
}
