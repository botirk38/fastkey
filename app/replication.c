#include "replication.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

ReplicationInfo *createReplicationInfo(const char *host, int port) {
  ReplicationInfo *repl_info = malloc(sizeof(ReplicationInfo));

  repl_info->master_info = malloc(sizeof(MasterInfo));
  repl_info->master_info->host = strdup(host);
  repl_info->master_info->port = port;
  repl_info->master_info->fd = -1;

  repl_info->replication_id =
      strdup("8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb");
  repl_info->repl_offset = 0;

  return repl_info;
}

int startReplication(MasterInfo *master_info) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  struct sockaddr_in master_addr;
  memset(&master_addr, 0, sizeof(master_addr));
  master_addr.sin_family = AF_INET;
  master_addr.sin_port = htons(master_info->port);
  inet_pton(AF_INET, master_info->host, &master_addr.sin_addr);

  if (connect(fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0) {
    close(fd);
    return -1;
  }

  const char *ping_cmd = "*1\r\n$4\r\nPING\r\n";
  write(fd, ping_cmd, strlen(ping_cmd));

  master_info->fd = fd;
  return 0;
}

void freeReplicationInfo(ReplicationInfo *repl_info) {
  if (repl_info) {
    if (repl_info->master_info) {
      if (repl_info->master_info->fd >= 0) {
        close(repl_info->master_info->fd);
      }
      free(repl_info->master_info->host);
      free(repl_info->master_info);
    }
    free(repl_info->replication_id);
    free(repl_info);
  }
}
