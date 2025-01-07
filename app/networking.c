#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int initServerSocket(redisServer *server) {
  int server_fd;
  struct sockaddr_in server_addr;

  // Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
    return -1;
  }

  // Set SO_REUSEADDR option
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    fprintf(stderr, "setsockopt SO_REUSEADDR failed: %s\n", strerror(errno));
    close(server_fd);
    return -1;
  }

  // Prepare server address structure
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server->port);

  if (inet_pton(AF_INET, server->bindaddr, &server_addr.sin_addr) <= 0) {
    fprintf(stderr, "Invalid address: %s\n", strerror(errno));
    close(server_fd);
    return -1;
  }

  // Bind socket
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    fprintf(stderr, "Bind failed: %s\n", strerror(errno));
    close(server_fd);
    return -1;
  }

  // Listen for connections
  if (listen(server_fd, server->tcp_backlog) < 0) {
    fprintf(stderr, "Listen failed: %s\n", strerror(errno));
    close(server_fd);
    return -1;
  }

  server->fd = server_fd;
  return 0;
}

int acceptClient(redisServer *server) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  int client_fd =
      accept(server->fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      fprintf(stderr, "Accept failed: %s\n", strerror(errno));
    }
    return -1;
  }

  // Set client socket to non-blocking mode
  int flags = fcntl(client_fd, F_GETFL, 0);
  if (flags == -1) {
    fprintf(stderr, "Failed to get socket flags: %s\n", strerror(errno));
    close(client_fd);
    return -1;
  }

  if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    fprintf(stderr, "Failed to set non-blocking mode: %s\n", strerror(errno));
    close(client_fd);
    return -1;
  }

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

  return client_fd;
}

int sendReply(redisServer *server, int client_fd, char *reply) {
  size_t len = strlen(reply);
  ssize_t sent = send(client_fd, reply, len, 0);

  if (sent < 0) {
    fprintf(stderr, "Send failed: %s\n", strerror(errno));
    return -1;
  }

  if ((size_t)sent < len) {
    // Partial send - in a real implementation, we'd buffer the remaining data
    fprintf(stderr, "Partial send occurred\n");
    return -1;
  }

  return 0;
}

void closeClientConnection(redisServer *server, int client_fd) {
  close(client_fd);
  server->clients_count--;
  printf("Client disconnected. Total clients: %d\n", server->clients_count);
}
