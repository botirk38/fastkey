#ifndef REPLICATION_H
#define REPLICATION_H

typedef struct {
  char *host;
  int port;
  int fd;
} MasterInfo;

typedef struct {
  MasterInfo *master_info;
  char *replication_id;
  long long repl_offset;
} ReplicationInfo;

// Creates replication info for a replica
ReplicationInfo *createReplicationInfo(const char *host, int port);

// Initiates connection to master and starts handshake
int startReplication(MasterInfo *repl_info, int listening_port);

// Frees replication resources
void freeReplicationInfo(ReplicationInfo *repl_info);

#endif
