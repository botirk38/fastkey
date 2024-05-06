#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define MAX_DIR_SIZE 256
#define MAX_FILE_NAME_SIZE 256

typedef struct {
  int port;
  char *masterHost;
  int masterPort;
  bool isSlave;

} Config;

typedef struct {

  char dir[MAX_DIR_SIZE];
  char dbFileName[MAX_FILE_NAME_SIZE];

} RDBConfig;

typedef struct {
  Config serverConfig;
  RDBConfig rdbConfig;

} Configs;


Configs parse_cli_args(int argc, char **argv);

#endif // CONFIG_H
