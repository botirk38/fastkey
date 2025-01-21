#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ServerConfig *parseConfig(int argc, char *argv[]) {
  ServerConfig *config = calloc(1, sizeof(ServerConfig));

  // Set defaults
  config->dir = strdup("/tmp");
  config->dbfilename = strdup("dump.rdb");
  config->port = 6379;
  config->bindaddr = strdup("127.0.0.1");
  config->is_replica = false;
  config->master_host = NULL;
  config->master_port = 0;

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
    } else if (strcmp(argv[i], "--replicaof") == 0 && i + 1 < argc) {
      config->is_replica = true;

      char *replicaof = strdup(argv[i + 1]);
      char *space = strchr(replicaof, ' ');
      if (space) {
        *space = '\0';
        config->master_host = strdup(replicaof);
        config->master_port = atoi(space + 1);
      }
      free(replicaof);
      i++;

      printf("Config: is_replica=%d, master_host=%s, master_port=%d\n",
             config->is_replica, config->master_host, config->master_port);
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
  free(config->master_host);
  free(config);
}
