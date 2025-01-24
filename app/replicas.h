#ifndef REPLICAS_H
#define REPLICAS_H

#include "resp.h"
#include "server.h"

void initReplicaList(RedisServer *server);
void addReplica(RedisServer *server, int fd);
void removeReplica(RedisServer *server, int fd);
void freeReplicas(RedisServer *server);
void propagateCommand(RedisServer *server, RespValue *command);

#endif
