/**
 * Author: Jeremy Wood
 */

#ifndef CSNAKE_SOCKET_H
#define CSNAKE_SOCKET_H

int connect_socket(const char *host, unsigned short port_num);
int listen_socket(const char *host, unsigned short port_num);

ssize_t ssend(int fd, void *message, size_t size);
ssize_t srecv(int fd, void *buffer, size_t size);

#endif //CSNAKE_SOCKET_H
