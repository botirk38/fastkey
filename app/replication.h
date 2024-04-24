#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdbool.h>




int establishConnection(const char *host, int port);
void closeConnection(int sockfd);

void sendPing(int sockfd);
void sendReplConf(int sockfd, int port);
void sendReplConfCapaPsync2(int sockfd);
void sendPsync(int sockfd);
void sendRDBFile(int sockfd);

bool startReplication(const char *masterHost, int masterPort, int port);
bool handShakeSuccess(int sockfd, int port);
void* skipRDBAndFullResync(void* args);
bool waitForOk(int sockfd);
bool waitForPong(int sockfd);

extern int master_fd;




#endif // REPLICATION_H

