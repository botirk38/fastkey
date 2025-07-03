#include "client_handler.h"
#include "config.h"
#include "logger.h"
#include "networking.h"
#include "server.h"
#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static volatile int g_running = 1;
static ThreadPool *g_pool = NULL;
static RedisServer *g_server = NULL;
static ServerConfig *g_config = NULL;

void cleanup_resources(void) {
  if (g_pool) {
    threadPoolDestroy(g_pool);
    g_pool = NULL;
  }
  if (g_server) {
    freeServer(g_server);
    g_server = NULL;
  }
  if (g_config) {
    freeConfig(g_config);
    g_config = NULL;
  }
  logger_close();
}

void signal_handler(int sig) {
  g_running = 0;
  cleanup_resources();
  exit(0);
}

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  g_config = parseConfig(argc, argv);
  if (!g_config) {
    fprintf(stderr, "Failed to parse configuration\n");
    return 1;
  }

  g_server = createServer(g_config);
  if (!g_server) {
    fprintf(stderr, "Failed to create server\n");
    freeConfig(g_config);
    return 1;
  }

  if (initServer(g_server) != 0) {
    fprintf(stderr, "Failed to initialize server\n");
    freeServer(g_server);
    freeConfig(g_config);
    return 1;
  }

  logger_init("app.log", LOG_TRACE);

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  g_pool = createThreadPool(8);
  if (!g_pool) {
    fprintf(stderr, "Failed to create thread pool\n");
    cleanup_resources();
    return 1;
  }

  printf("Redis server listening on port %d with %d worker threads\n",
         g_server->port, num_cores);

  while (g_running) {
    int clientFd = acceptClient(g_server);
    LOG_TRACE("Client FD %d ", clientFd);
    if (clientFd >= 0) {
      ClientHandlerArg *arg = malloc(sizeof(ClientHandlerArg));
      if (!arg) {
        LOG_ERROR("Failed to allocate memory for client handler arg");
        close(clientFd);
        continue;
      }
      arg->server = g_server;
      arg->clientFd = clientFd;
      if (threadPoolAdd(g_pool, handleClientConnection, arg) != 0) {
        LOG_ERROR("Failed to add client to thread pool");
        free(arg);
        close(clientFd);
      }
    }
  }

  cleanup_resources();
  return 0;
}
