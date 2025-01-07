#include "networking.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // Create and initialize server
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

  // Main server loop
  while (1) {
    // Accept new connections
    int client_fd = acceptClient(server);
    if (client_fd > 0) {
      printf("New client connected\n");
      server->clients_count++;
    }

    // Read client input (we'll ignore it for now)
    char buffer[REGULAR_BUFFER_SIZE];
    read(client_fd, buffer, REGULAR_BUFFER_SIZE);

    // Send PONG response in RESP format
    const char *response = "+PONG\r\n";
    if (sendReply(server, client_fd, (char *)response) < 0) {
      fprintf(stderr, "Failed to send response\n");
    }

    // Run periodic tasks
    serverCron(server);
  }

  freeServer(server);
  return 0;
}
