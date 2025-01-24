#ifndef REPLICATION_H
#define REPLICATION_H

#define INITIAL_REPLICA_CAPACITY 16

#include <stddef.h>

typedef struct {
  char *host;
  int port;
  int fd;
} MasterInfo;

typedef struct {
  int *replica_fds;
  size_t replica_count;
  size_t replica_capacity;
} Replicas;

typedef struct {
  MasterInfo *master_info;
  char *replication_id;
  long long repl_offset;
  Replicas *replicas; // Only if server is master
} ReplicationInfo;

// Creates replication info for a replica
ReplicationInfo *createReplicationInfo(const char *host, int port);

// Initiates connection to master and starts handshake
int startReplication(MasterInfo *repl_info, int listening_port);

// Frees replication resources
void freeReplicationInfo(ReplicationInfo *repl_info);

#endif
