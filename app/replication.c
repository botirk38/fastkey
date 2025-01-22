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

  // Send PING command and wait for response
  const char *ping_elements[] = {"PING"};
  char *ping_cmd = createRespArray(ping_elements, 1);
  printf("Sending PING command to master\n");

  if (writeExactly(fd, ping_cmd, strlen(ping_cmd)) < 0) {
    printf("Failed to send PING command\n");
    free(ping_cmd);
    close(fd);
    return -1;
  }
  printf("PING command sent successfully\n");
  free(ping_cmd);

  // Read PING response
  char response[1024];
  if (readExactly(fd, response, 5) < 0) { // Read "+PONG\r\n"
    printf("Failed to read PING response\n");
    close(fd);
    return -1;
  }

  // Send REPLCONF listening-port
  const char *replconf1_elements[] = {"REPLCONF", "listening-port", "6380"};
  char *replconf1_cmd = createRespArray(replconf1_elements, 3);
  printf("Sending REPLCONF listening-port command\n");

  if (writeExactly(fd, replconf1_cmd, strlen(replconf1_cmd)) < 0) {
    printf("Failed to send REPLCONF listening-port command\n");
    free(replconf1_cmd);
    close(fd);
    return -1;
  }
  printf("REPLCONF listening-port command sent successfully\n");
  free(replconf1_cmd);

  // Read REPLCONF response
  if (readExactly(fd, response, 5) < 0) { // Read "+OK\r\n"
    printf("Failed to read REPLCONF response\n");
    close(fd);
    return -1;
  }

  // Send REPLCONF capabilities
  const char *replconf2_elements[] = {"REPLCONF", "capa", "psync2"};
  char *replconf2_cmd = createRespArray(replconf2_elements, 3);
  printf("Sending REPLCONF capabilities command\n");

  if (writeExactly(fd, replconf2_cmd, strlen(replconf2_cmd)) < 0) {
    printf("Failed to send REPLCONF capabilities command\n");
    free(replconf2_cmd);
    close(fd);
    return -1;
  }
  printf("REPLCONF capabilities command sent successfully\n");
  free(replconf2_cmd);

  // Read final response
  if (readExactly(fd, response, 5) < 0) { // Read "+OK\r\n"
    printf("Failed to read final response\n");
    close(fd);
    return -1;
  }

  master_info->fd = fd;
  printf("Replication setup completed successfully\n");

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
