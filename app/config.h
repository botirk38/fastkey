#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  int port;

} Config;

Config parse_cli_args(int argc, char **argv);

#endif // CONFIG_H
