#include "replication.h"
#include "networking.h"
#include "resp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int startReplication(MasterInfo *master_info, int listening_port) {
  printf("Starting replication with master %s:%d\n", master_info->host,
         master_info->port);

  int fd = connectToHost(master_info->host, master_info->port);
  if (fd < 0) {
    printf("Failed to connect to master %s:%d\n", master_info->host,
           master_info->port);
    return -1;
  }
  printf("Successfully connected to master. Socket fd: %d\n", fd);

  // PING handshake
  const char *ping_elements[] = {"PING"};
  char *ping_cmd = createRespArray(ping_elements, 1);
  if (writeExactly(fd, ping_cmd, strlen(ping_cmd)) < 0) {
    free(ping_cmd);
    close(fd);
    return -1;
  }
  free(ping_cmd);

  // Wait for PONG
  char response[1024];
  ssize_t n = readLine(fd, response, sizeof(response));
  if (n <= 0 || strncmp(response, "+PONG", 5) != 0) {
    close(fd);
    return -1;
  }

  // REPLCONF listening-port
  char port_str[32];
  snprintf(port_str, sizeof(port_str), "%d", listening_port);
  const char *replconf1_elements[] = {"REPLCONF", "listening-port", port_str};
  char *replconf1_cmd = createRespArray(replconf1_elements, 3);
  if (writeExactly(fd, replconf1_cmd, strlen(replconf1_cmd)) < 0) {
    free(replconf1_cmd);
    close(fd);
    return -1;
  }
  free(replconf1_cmd);

  // Wait for OK
  n = readLine(fd, response, sizeof(response));
  if (n <= 0 || strncmp(response, "+OK", 3) != 0) {
    close(fd);
    return -1;
  }

  // REPLCONF capabilities
  const char *replconf2_elements[] = {"REPLCONF", "capa", "psync2"};
  char *replconf2_cmd = createRespArray(replconf2_elements, 3);
  if (writeExactly(fd, replconf2_cmd, strlen(replconf2_cmd)) < 0) {
    free(replconf2_cmd);
    close(fd);
    return -1;
  }
  free(replconf2_cmd);

  // Wait for OK
  n = readLine(fd, response, sizeof(response));
  if (n <= 0 || strncmp(response, "+OK", 3) != 0) {
    close(fd);
    return -1;
  }

  // PSYNC
  const char *psync_elements[] = {"PSYNC", "?", "-1"};
  char *psync_cmd = createRespArray(psync_elements, 3);
  if (writeExactly(fd, psync_cmd, strlen(psync_cmd)) < 0) {
    free(psync_cmd);
    close(fd);
    return -1;
  }
  free(psync_cmd);

  // Read FULLRESYNC response
  n = readLine(fd, response, sizeof(response));
  if (n <= 0 || strncmp(response, "+FULLRESYNC", 11) != 0) {
    close(fd);
    return -1;
  }

  // Read RDB size
  n = readLine(fd, response, sizeof(response));
  if (n <= 0 || response[0] != '$') {
    close(fd);
    return -1;
  }

  int rdb_size = atoi(response + 1);
  char *rdb_data = malloc(rdb_size);
  int total_read = 0;

  // Read full RDB file
  while (total_read < rdb_size) {
    n = read(fd, rdb_data + total_read, rdb_size - total_read);
    if (n <= 0) {
      free(rdb_data);
      close(fd);
      return -1;
    }
    total_read += n;
  }

  // Store RDB data or process it as needed
  free(rdb_data);

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
