#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdbool.h>

int establishConnection(const char *host, int port);
void closeConnection(int sockfd);
bool sendPing(int sockfd);
bool startReplication(const char *masterHost, int masterPort);

#endif // REPLICATION_H

