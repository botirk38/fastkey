#ifndef REDIS_NETWORKING_H
#define REDIS_NETWORKING_H

#include "server.h"
#include <arpa/inet.h> // For inet_pton
#include <errno.h>
#include <fcntl.h>      // For fcntl(), F_GETFL, F_SETFL
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h> // For socket operations
#include <unistd.h>     // For close()

#define REGULAR_BUFFER_SIZE 1024

// Networking setup
int initServerSocket(RedisServer *server);
int acceptClient(RedisServer *server);
void closeClientConnection(RedisServer *server, int client_fd);

// Protocol handling
int processInputBuffer(RedisServer *server, int client_fd);
int readLine(int fd, char *buffer, size_t max_len);

int sendReply(const RedisServer *server, const int client_fd,
              const char *reply);

int connectToHost(const char *host, int port);
int readExactly(int fd, char *buffer, size_t n);
int writeExactly(int fd, const char *buffer, size_t n);

#endif
