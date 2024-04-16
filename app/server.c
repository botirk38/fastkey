#include "./network/thread_pool.h"
#include "command-handler.h"
#include "parser.h"
#include "utils/KeyValueStore.h"
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

volatile sig_atomic_t server_running = 1;
KeyValueStore store;

int main() {
  setbuf(stdout, NULL);
  printf("Logs from your program will appear here!\n");

  int server_fd = create_server_socket();
  if (server_fd == -1)
    return 1;

  if (bind_to_port(server_fd, 6379) == -1) {
    cleanup(server_fd);
    cleanup_thread_pool();
    return 1;
  }

  if (start_listening(server_fd, 5) == -1) {
    cleanup(server_fd);
    cleanup_thread_pool();
    return 1;
  }

  init_thread_pool();
  initKeyValueStore(&store);

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

  // Wait for the accept thread to finish (e.g., on server shutdown)
  pthread_join(accept_thread, NULL);
  pthread_join(keyExpiryThread, NULL);

  freeKeyValueStore(&store);
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
    char buffer[BUFFER_SIZE] = {0};

    printf("Waiting to receive data...\n");

    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1,
                                  0); // -1 to leave space for null terminator

    if (bytes_received <= 0) {
      if (bytes_received == 0)
        printf("Client closed connection\n");
      else
        perror("Receive failed");
      close(client_fd);
      return;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data

    printf("Received: %s\n", buffer);

    RespCommand *command = parseCommand(buffer);

    printf("Command: %s\n", command->command);

    for (int i = 0; i < command->numArgs; i++) {
      printf("Arg %d: %s\n", i, command->args[i]);
    }

    char *response = handleCommand(command->command, command->args, command->numArgs);

    printf("Response: %s\n", response);

    send(client_fd, response, strlen(response), 0);
  }

  close(client_fd);
}

void *worker_func(void *arg) {

  while (1) {
    int client_fd = dequeue_client();
    handle_client(client_fd);
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
