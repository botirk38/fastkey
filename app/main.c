#include "event_loop.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  RedisServer *server = createServer();
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

  EventLoop *loop = createEventLoop();
  eventLoopStart(loop, server);

  return 0;
}
