#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "event_loop.h"

int main(void) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    redisServer *server = createServer();
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    if (initServer(server) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        freeServer(server);
        return 1;
    }
    
    printf("Redis server listening on port %d\n", server->port);
    
    event_loop* loop = create_event_loop();
    event_loop_start(loop, server);
    
    return 0;
}

