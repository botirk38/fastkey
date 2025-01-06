#include "./network/thread_pool.h"
#include "command-handler.h"
#include "config.h"
#include "parser.h"
#include "replication.h"
#include "utils/KeyValueStore.h"
#include "utils/ReplicaStore.h"
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define NEWLINE_CHARACTERS_COMMAND 7

int create_server_socket();
int bind_to_port(int server_fd, int port);
int start_listening(int server_fd, int backlog);
void *accept_connections(void *arg);
void cleanup(int server_fd);
void handle_client(int client_fd);
void *worker_func(void *arg);
void init_thread_pool();
void handle_sigint(int sig);
void cleanup_thread_pool();
void *deleteExpiredKeysWorker(void *arg);
void handle_master(int sockfd);

volatile sig_atomic_t server_running = 1;
Config config;
Replicas replicas;
extern RDBConfig rdbConfig;
KeyValueStore store;

int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);
  printf("Logs from your program will appear here!\n");

  Configs configs = parse_cli_args(argc, argv);

  config = configs.serverConfig;
  rdbConfig = configs.rdbConfig;

  int server_fd = create_server_socket();
  if (server_fd == -1)
    return 1;

  if (bind_to_port(server_fd, config.port) == -1) {
    cleanup(server_fd);
    cleanup_thread_pool();
    return 1;
  }

  if (start_listening(server_fd, 128) == -1) {
    cleanup(server_fd);
    cleanup_thread_pool();
    return 1;
  }

  init_thread_pool();
  initKeyValueStore(&store);
  initReplicas(&replicas);

  if (rdbConfig.dir[0] != '\0' && rdbConfig.dbFileName[0] != '\0') {

    parseRDBFile(rdbConfig.dbFileName, rdbConfig.dir, &store);
  }

  pthread_t accept_thread;
  if (pthread_create(&accept_thread, NULL, accept_connections, &server_fd) !=
      0) {
    perror("Failed to create accept thread");
    cleanup(server_fd);
    return 1;
  }

  pthread_t keyExpiryThread;
  if (pthread_create(&keyExpiryThread, NULL, deleteExpiredKeysWorker, &store) !=
      0) {
    perror("Failed to create key expiry thread");
    cleanup(server_fd);
    return 1;
  }

  if (config.isSlave) {

    if (startReplication(config.masterHost, config.masterPort, config.port) ==
        false) {
      printf("Failed to start replication\n");
      cleanup(server_fd);
      cleanup_thread_pool();
      return 1;
    }
    enqueue_client(master_fd);
  }

  // Wait for the accept thread to finish (e.g., on server shutdown)
  pthread_join(accept_thread, NULL);
  pthread_join(keyExpiryThread, NULL);

  printf("Cleaning up...\n");

  freeKeyValueStore(&store);
  freeReplicas(&replicas);
  cleanup_thread_pool();
  cleanup(server_fd);

  return 0;
}

void *deleteExpiredKeysWorker(void *arg) {
  KeyValueStore *store = (KeyValueStore *)arg;
  while (server_running) {
    deleteExpiredKeys(store);
    sleep(1);
  }
  return NULL;
}

int create_server_socket() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return -1;
  }

  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return -1;
  }

  return server_fd;
}

int bind_to_port(int server_fd, int port) {
  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return -1;
  }

  return 0;
}

int start_listening(int server_fd, int backlog) {
  if (listen(server_fd, backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return -1;
  }
  return 0;
}

void *accept_connections(void *arg) {
  int server_fd = *(int *)arg;
  printf("Waiting for clients to connect...\n");

  while (server_running) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd == -1) {
      if (errno == EINTR) {
        continue; // Interrupted by signal
      }
      perror("Accept failed");
      continue;
    }

    printf("Client connected successfully!\n");
    enqueue_client(client_fd);
  }

  return NULL;
}

void cleanup(int server_fd) { close(server_fd); }

void handle_client(int client_fd) {

  while (1) {

    printf("Client fd: %d\n", client_fd);

    char buffer[BUFFER_SIZE] = {0};

    printf("Waiting to receive data...\n");

    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1,
                                  0); // -1 to leave space for null terminator

    if (bytes_received <= 0) {
      if (bytes_received == 0) {
        printf("Client closed connection\n");
      } else
        perror("Receive failed");
      return;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data

    printf("Received: %s\n", buffer);

    RespCommand *command = parseCommand(buffer);

    if (!command) {
      printf("Failed to parse command\n");
      continue;
    }

    printf("Command in Master: %s\n", command->command);

    if (strcmp(command->command, "WAIT") == 0) {

      printf("Propagating command to replicas WAIT\n");

      propagateCommandToReplicas(
          &replicas, "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n");

      // Allocate memory for the string representation of the integer
      char *replicaCountStr = malloc(10); // Enough to hold all digits of an int
      if (replicaCountStr == NULL) {
        perror("Failed to allocate memory for replica count string");
        return; // or handle error more appropriately
      }
      snprintf(replicaCountStr, 10, "%d", replicas.numReplicas);
      command->args[2] = replicaCountStr;
      command->numArgs++;
    }

    for (int i = 0; i < command->numArgs; i++) {
      printf("Arg %d: %s\n", i, command->args[i]);
    }

    char *response = handleCommand(command->command, command->args,
                                   command->numArgs, config.isSlave);

    if (!config.isSlave) {
      printf("Adding replica\n");
      if (strcmp(command->command, "PSYNC") == 0) {

        addReplica(&replicas, client_fd, config.masterHost, config.masterPort);
      }

      printf("Replicas: %d\n", replicas.numReplicas);

      if (isWriteCommand(command->command) && replicas.numReplicas > 0) {
        printf("Propagating command to replicas\n");
        propagateCommandToReplicas(&replicas, buffer);
      }
    }

    printf("Response: %s\n", response);

    if (send(client_fd, response, strlen(response), 0) == -1) {
      perror("Send failed \n");
      return;
    }
  }
  printf("Closing client connection\n");
  close(client_fd);
}

char *substr(const char *input, int start, int end, int input_length) {
  if (input == NULL) {
    fprintf(stderr, "Invalid input string.\n");
    return NULL;
  }

  if (start < 0 || start >= input_length || end < start || end > input_length) {
    fprintf(stderr, "Start or end position out of bounds.\n");
    return NULL;
  }

  int length = end - start; // Calculate the length from start to end
  char *result = malloc(length + 1);
  if (result == NULL) {
    perror("Failed to allocate memory for substring");
    return NULL;
  }

  memcpy(result, input + start, length);
  result[length] = '\0'; // Ensure the substring is null-terminated

  return result;
}

void handle_master(int master_fd) {
  char buffer[BUFFER_SIZE];
  ssize_t n = 0;

  if (master_fd == -1) {
    fprintf(stderr, "Master connection is closed\n");
    return;
  }

  while ((n = recv(master_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
    buffer[n] = '\0'; // Null-terminate the string
    size_t start = 0;

    while (start < n) {
      if (buffer[start] != '*') {
        start++;
        continue; // skip until finding the start of a command
      }

      size_t end = start + 1;
      // Find the end of the current command
      while (end < n && buffer[end] != '*') {
        end++;
      }
      if (end < n || buffer[end - 1] == '\n') {
        RespCommand *command = parseCommand(buffer + start);

        if (command) {

          printf("Command in Slave: %s\n", command->command);

          char *result = handleCommand(command->command, command->args,
                                       command->numArgs, 1);

          if (result) {
            printf("Result in Slave: %s\n", result);

            if (strcmp(command->command, "REPLCONF") == 0 &&
                strcmp(command->args[0], "GETACK") == 0) {

              if (send(master_fd, result, strlen(result), 0) != 0) {
                fprintf(stderr, "Send failed to master");
              }

              printf("Sent to master");

              break;
            }
            free(result);
          }
        }

        start = end;
      } else {
        // Should not reach here if all commands are complete
        break;
      }
    }
  }

  if (n < 0) {
    perror("Receive failed");
  }

  close(master_fd); // Close the socket
}

void *worker_func(void *arg) {

  while (1) {
    int client_fd = dequeue_client();
    if (client_fd == master_fd && config.isSlave) {
      handle_master(client_fd);
    } else {
      handle_client(client_fd);
    }
  }
}

void init_thread_pool() {
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&workers[i], NULL, worker_func, NULL);
  }
}

void cleanup_thread_pool() {
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(workers[i], NULL);
  }
}

void handle_sigint(int sig) { server_running = 0; }
