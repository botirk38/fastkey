#ifndef REDIS_NETWORKING_H
#define REDIS_NETWORKING_H

#include "server.h"
#include <arpa/inet.h>  // For inet_pton
#include <fcntl.h>      // For fcntl(), F_GETFL, F_SETFL
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h> // For socket operations
#include <unistd.h>     // For close()

#define REGULAR_BUFFER_SIZE 1024

// Networking setup
int initServerSocket(redisServer *server);
int acceptClient(redisServer *server);
void closeClientConnection(redisServer *server, int client_fd);

// Protocol handling
int processInputBuffer(redisServer *server, int client_fd);
int sendReply(redisServer *server, int client_fd, char *reply);

#endif
