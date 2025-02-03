#include "client_handler.h"
#include "config.h"
#include "logger.h"
#include "networking.h"
#include "server.h"
#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  ServerConfig *server_config = parseConfig(argc, argv);
  RedisServer *server = createServer(server_config);

  if (!server) {
    fprintf(stderr, "Failed to create server\n");
    return 1;
  }

  if (initServer(server) != 0) {
    fprintf(stderr, "Failed to initialize server\n");
    freeServer(server);
    return 1;
  }

  logger_init("app.log", LOG_TRACE);

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool *pool = createThreadPool(8);

  printf("Redis server listening on port %d with %d worker threads\n",
         server->port, num_cores);

  while (1) {
    int clientFd = acceptClient(server);
    LOG_TRACE("Client FD %d ", clientFd);
    if (clientFd >= 0) {
      ClientHandlerArg *arg = malloc(sizeof(ClientHandlerArg));
      arg->server = server;
      arg->clientFd = clientFd;
      threadPoolAdd(pool, handleClientConnection, arg);
    }
  }

  threadPoolDestroy(pool);
  return 0;
}
