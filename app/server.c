#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


#define BUFFER_SIZE 1024

int create_server_socket();
int bind_to_port(int server_fd, int port);
int start_listening(int server_fd, int backlog);
void accept_connections(int server_fd);
void cleanup(int server_fd);
void handle_client(int client_fd);

int main() {
    setbuf(stdout, NULL);
    printf("Logs from your program will appear here!\n");

    int server_fd = create_server_socket();
    if (server_fd == -1) return 1;

    if (bind_to_port(server_fd, 6379) == -1) {
        cleanup(server_fd);
        return 1;
    }

    if (start_listening(server_fd, 5) == -1) {
        cleanup(server_fd);
        return 1;
    }

    accept_connections(server_fd);
    cleanup(server_fd);

    return 0;
}

int create_server_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return -1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return -1;
    }

    return server_fd;
}

int bind_to_port(int server_fd, int port) {
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(INADDR_ANY) },
    };

    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
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

void accept_connections(int server_fd) {
    printf("Waiting for a client to connect...\n");
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
	int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

	if(client_fd == -1) {
		perror("Accept failed: ");
		return;
	}

	printf("Client connected successfully!\n");


	handle_client(client_fd);


}

void cleanup(int server_fd) {
    close(server_fd);
}

void handle_client(int client_fd) {

	while(1) {

	char buffer[BUFFER_SIZE];

	ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

	if(bytes_received == -1) {
		perror("Receive failed: ");
		return;
	}

	buffer[bytes_received] = '\0';


	printf("Received: %s\n", buffer);


	char response[] = "+PONG\r\n";

	ssize_t bytes_sent = send(client_fd, response, strlen(response), 0);

	if(bytes_sent == -1) {
		perror("Send failed: ");
		return;
	}

	printf("Sent: %s\n", response);

	}

	close(client_fd);

	
}

