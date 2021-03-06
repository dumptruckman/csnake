/**
 * Author: Jeremy Wood
 */

#ifndef CSNAKE_CLIENT_H
#define CSNAKE_CLIENT_H

#include <pthread.h>
#include "snake.h"

typedef struct {
    int client_socket;
    pthread_t client_thread;
    snake_t snake;
} client_t;

void run_client(char *host, unsigned short port_num);

#endif //CSNAKE_CLIENT_H
