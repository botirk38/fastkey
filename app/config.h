#ifndef CONFIG_H
#define CONFIG_H

typedef struct ServerConfig {
  char *dir;
  char *dbfilename;
  int port;
  char *bindaddr;
} ServerConfig;

ServerConfig *parseConfig(int argc, char *argv[]);
void freeConfig(ServerConfig *config);

#endif
