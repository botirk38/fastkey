#include "ReplicaStore.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

void initReplicas(Replicas *replicas) {
  replicas->numReplicas= 0;
  replicas->replicas = (Replica *)malloc(MAX_REPLICAS * sizeof(Replica));
}

bool isReplicasEmpty(Replicas *replicas) {
    
  return replicas ->numReplicas >0;
}

void addReplica(Replicas *replicas, int fd, const char *host, int port) {
  if (replicas->maxReplicas == MAX_REPLICAS) {
    printf("Max replicas reached\n");
    return;
  }

  replicas->replicas[replicas->numReplicas].host = host;
  replicas->replicas[replicas->numReplicas].port = port;
  replicas->replicas[replicas->numReplicas].fd = fd;
  replicas->numReplicas++;

  printf("Replica added\n");
}

void removeReplica(Replicas *replicas, int fd) {
  for (int i = 0; i < replicas->numReplicas; i++) {
    if (replicas->replicas[i].fd == fd) {
      for (int j = i; j < replicas->numReplicas - 1; j++) {
        replicas->replicas[j] = replicas->replicas[j + 1];
      }
      replicas->numReplicas--;
      printf("Replica removed\n");
      return;
    }
  }

  printf("Replica not found\n");
}

void freeReplicas(Replicas *replicas) {
  free(replicas->replicas);
}

void propagateCommandToReplicas(Replicas *replicas, char* buffer) {
  
  for (int i = 0; i < replicas->numReplicas; i++) {
    printf("Propagating command to replica %d\n", i);
    // Send command to replica
    
    send(replicas->replicas[i].fd, buffer, strlen(buffer), 0);
    

  }

}

bool isReplica(Replicas *replicas, int fd) {
  for (int i = 0; i < replicas->numReplicas; i++) {
    if (replicas->replicas[i].fd == fd) {
      return true;
    }
  }

  return false;
}
