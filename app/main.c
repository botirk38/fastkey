#include "networking.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(void) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  redisServer *server = createServer();
  if (!server) {
    fprintf(stderr, "Failed to create server\n");
    return 1;
  }

  if (initServer(server) != 0) {
    fprintf(stderr, "Failed to initialize server\n");
    freeServer(server);
    return 1;
  }

  printf("Redis server listening on port %d\n", server->port);

  int client_fd = acceptClient(server);
  if (client_fd < 0) {
    fprintf(stderr, "Failed to accept client\n");
    freeServer(server);
    return 1;
  }

  char buffer[BUFFER_SIZE];
  const char *response = "+PONG\r\n";

  while (1) {
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
      if (sendReply(server, client_fd, (char *)response) < 0) {
        break;
      }
    } else if (bytes_read == 0) {
      // Client disconnected
      break;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      // Error occurred
      break;
    }
  }

  closeClientConnection(server, client_fd);
  freeServer(server);
  return 0;
}

