#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct {
  int port;
  char* masterHost;
  int masterPort;
  bool isSlave;

} Config;

Config parse_cli_args(int argc, char **argv);

#endif // CONFIG_H
