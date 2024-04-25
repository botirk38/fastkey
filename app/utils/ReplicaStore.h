#ifndef REPLICA_STORE_H
#define REPLICA_STORE_H

#include <stdbool.h>

#define MAX_REPLICAS 10

typedef struct {
  
  int fd;
  int port;
  const char* host;
} Replica;


typedef struct {

  Replica *replicas;
  int numReplicas;
  int maxReplicas;
  

} Replicas;


void initReplicas(Replicas *replicas);
void addReplica(Replicas *replicas, int fd, const char *host, int port);
void removeReplica(Replicas *replicas, int fd);
void freeReplicas(Replicas *replicas);
void propagateCommandToReplicas(Replicas *replicas, char*buffer);
bool isReplicasEmpty(Replicas *replicas);


#endif // !REPLICA_STORE_H
