#include "replication.h"
#include "utils/utils.h"
#include <arpa/inet.h> // for htons(), inet_pton()
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h> // for close()

int master_fd = -1;

int establishConnection(const char *host, int port) {
  struct sockaddr_in serv_addr;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  int reuse = 1;

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("SO_REUSEADDR failed");
    close(sockfd);
    return -1;
  }

  // Reuse the port if the connection is lost

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
    perror("SO_REUSEPORT failed");
    close(sockfd);
    return -1;
  }

  if (sockfd < 0) {
    perror("Socket creation error");
    return -1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
    perror("Invalid address / Address not supported");
    close(sockfd);
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Connection Failed");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

void closeConnection(int sockfd) { close(sockfd); }

void sendPing(int sockfd) {
  const char *pingMessage = "*1\r\n$4\r\nPING\r\n";
  ssize_t n = send(sockfd, pingMessage, strlen(pingMessage), 0);
}

void sendReplConf(int sockfd, int port) {
  char portStr[10];
  snprintf(portStr, sizeof(portStr), "%d",
           port); // Convert int to string for port

  char lenPortStr[10];
  snprintf(lenPortStr, sizeof(lenPortStr), "%d",
           (int)strlen(portStr)); // Get length of port string as string

  // Calculate the length of the command string dynamically
  int cmdLen = snprintf(
      NULL, 0, "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$%s\r\n%s\r\n",
      lenPortStr, portStr);

  char *cmd = malloc(cmdLen + 1); // Allocate memory for the command string
  if (cmd == NULL) {
    perror("Memory allocation failed");
    return;
  }

  // Construct the command string
  snprintf(cmd, cmdLen + 1,
           "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$%s\r\n%s\r\n",
           lenPortStr, portStr);

  // Send the command to the socket
  send(sockfd, cmd, cmdLen, 0);

  // Free the allocated memory
  free(cmd);
}

void sendReplConfCapaPsync2(int sockfd) {

  char *cmd = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";

  send(sockfd, cmd, strlen(cmd), 0);
}

void sendPsync(int sockfd) {
  char *cmd = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";

  send(sockfd, cmd, strlen(cmd), 0);
}

bool waitForOk(int sockfd) {
  char buffer[1024];
  ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);

  if (n < 0) {
    perror("Receive failed");
    return false;
  }

  buffer[n] = '\0';

  if (strcmp(buffer, "+OK\r\n") != 0) {
    return false;
  }

  return true;
}

bool waitForPong(int sockfd) {
  char buffer[1024];
  ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);

  if (n < 0) {
    perror("Receive failed");
    return false;
  }

  buffer[n] = '\0';

  if (strcmp(buffer, "+PONG\r\n") != 0) {
    return false;
  }

  return true;
}

bool startReplication(const char *masterHost, int masterPort, int port) {
  master_fd = establishConnection(masterHost, masterPort);

  if (master_fd < 0) {
    return false;
  }

  if (!handShakeSuccess(master_fd, port)) {
    closeConnection(master_fd);
    return false;
  }

  return true;
}

bool handShakeSuccess(int sockfd, int port) {

  char buffer[1024];
  pthread_t thread_id;

  sendPing(sockfd);

  if (!waitForPong(sockfd)) {
    closeConnection(sockfd);
    return false;
  }

  sendReplConf(sockfd, port);

  if (!waitForOk(sockfd)) {
    closeConnection(sockfd);
    return false;
  }

  sendReplConfCapaPsync2(sockfd);

  if (!waitForOk(sockfd)) {
    closeConnection(sockfd);
    return false;
  }

  sendPsync(sockfd);

  if (pthread_create(&thread_id, NULL, skipRDBAndFullResync, (void *)&sockfd) !=
      0) {
    perror("Failed to create thread");
    return false;
  }

  void *result;
  if (pthread_join(thread_id, &result) != 0) {
    perror("Failed to join thread");
    return false;
  }

  if (result == (void *)true) {
    printf("Successfully skipped RDB and full resync.\n");
    return true;
  } else {
    printf("Failed to skip RDB and full resync.\n");
    return false;
  }

}

void *skipRDBAndFullResync(void *sockfd_ptr) {
  int sockfd =
      *(int *)sockfd_ptr; // Cast and dereference the pointer to get the sockfd
  char c = 0;
  int rdb_len = 0; // Initialize rdb_len to avoid using uninitialized value

  while (recv(sockfd, &c, 1, 0) > 0 && c != '\n')
    ;

  if (recv(sockfd, &c, 1, 0) <= 0 || c != '$') {
    closeConnection(sockfd);
    return (void *)false; // Return false as a void pointer
  }

  while (recv(sockfd, &c, 1, 0) > 0 && c != '\r') {
    if (c >= '0' && c <= '9') {
      rdb_len = rdb_len * 10 + (c - '0');
    } else {
      closeConnection(sockfd);
      return (void *)false;
    }
  }

  // Skipping the RDB data
  int bytesToDiscard = rdb_len + 1;
  char discardBuffer[1024];
  recv(sockfd, discardBuffer, bytesToDiscard, 0);
  printf("Discarded buffer: %s\n", discardBuffer);

  return (void *)true; // Return true as a void pointer
}
