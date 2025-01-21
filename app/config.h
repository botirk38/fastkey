#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct ServerConfig {
  char *dir;
  char *dbfilename;
  int port;
  char *bindaddr;
  char *master_host;
  int master_port;
  bool is_replica;

} ServerConfig;

ServerConfig *parseConfig(int argc, char *argv[]);
void freeConfig(ServerConfig *config);

#endif
