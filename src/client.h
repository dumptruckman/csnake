//
// Created by Jeremy on 11/26/2017.
//

#ifndef CSNAKE_CLIENT_H
#define CSNAKE_CLIENT_H

#include <pthread.h>

typedef struct {
    int client_socket;
    pthread_t client_thread;
    int16_t x, y;
} client_t;

void run_client(char *host, unsigned short port_num);

#endif //CSNAKE_CLIENT_H
