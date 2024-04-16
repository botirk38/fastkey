#include "replication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>  // for htons(), inet_pton()
#include <unistd.h>     // for close()

int establishConnection(const char *host, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

void closeConnection(int sockfd) {
    close(sockfd);
}

bool sendPing(int sockfd) {
    const char *pingMessage = "*1\r\n$4\r\nPING\r\n";
    ssize_t n = write(sockfd, pingMessage, strlen(pingMessage));
    if (n < 0) {
        perror("Send failed");
        return false;
    }
    return true;
}

bool startReplication(const char *masterHost, int masterPort) {
    int sockfd = establishConnection(masterHost, masterPort);
    if (sockfd < 0) {
        return false;
    }

    if (!sendPing(sockfd)) {
        closeConnection(sockfd);
        return false;
    }

    // Assuming future steps would also use sockfd
    // if (!handshakeSuccess(sockfd)) {
    //     closeConnection(sockfd);
    //     return false;
    // }

    // Placeholder to keep the connection for further steps
    // closeConnection(sockfd);
    return true;
}

