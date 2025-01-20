#include "config.h"
#include <stdlib.h>
#include <string.h>

ServerConfig *parseConfig(int argc, char *argv[]) {
  ServerConfig *config = calloc(1, sizeof(ServerConfig));

  // Set defaults
  config->dir = strdup("/tmp");
  config->dbfilename = strdup("dump.rdb");
  config->port = 6379;
  config->bindaddr = strdup("127.0.0.1");

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
      free(config->dir);
      config->dir = strdup(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "--dbfilename") == 0 && i + 1 < argc) {
      free(config->dbfilename);
      config->dbfilename = strdup(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      config->port = atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
      free(config->bindaddr);
      config->bindaddr = strdup(argv[i + 1]);
      i++;
    }
  }

  return config;
}

void freeConfig(ServerConfig *config) {
  if (!config)
    return;
  free(config->dir);
  free(config->dbfilename);
  free(config->bindaddr);
  free(config);
}
