#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config parse_cli_args(int argc, char **argv) {

  for (int i = 1; i < argc; i++) {
    printf("argv[%d] = %s\n", i, argv[i]);
  }

  Config config = {config.port = 6379, config.masterHost = NULL, config.masterPort = 0, config.isSlave = false};

  for (int i = 1; i < argc; i++) {

    if (strcmp(argv[i], "--port") == 0) {
      if (argv[i + 1] == NULL) {
        printf("Invalid arguments for --port\n");
        exit(1);
      }

      config.port = atoi(argv[i + 1]);
    }

    if (strcmp(argv[i], "--replicaof") == 0) {
      if (argv[i + 1] == NULL || argv[i + 2] == NULL) {
        printf("Invalid arguments for --replicaof\n");
        exit(1);
      }

      if (strcmp(argv[i + 1], "localhost") == 0) {
        config.masterHost = strdup("127.0.0.1");
      }

      config.masterPort = atoi(argv[i + 2]);
      config.isSlave = true;
    }
  }

  return config;
}
