//
// Created by Jeremy on 11/14/2017.
//

#ifndef CSNAKE_SOCKET_H
#define CSNAKE_SOCKET_H

int connect_socket(const char *host, unsigned short port_num);
int listen_socket(const char *host, unsigned short port_num);

#endif //CSNAKE_SOCKET_H
