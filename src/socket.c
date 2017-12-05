//
// Created by Jeremy on 11/14/2017.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "log.h"
#include "socket.h"

static void configure_connect_hints(struct addrinfo *hints) {
    memset(hints, 0, sizeof(struct addrinfo));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_NUMERICSERV;
}

// Returns the socket fd if sucessful or -1 otherwise.
static int resolve_host(struct addrinfo *address) {
    int socket_fd;

    do {
        socket_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket_fd < 0) {
            log_debug("resolve_host: Could not establish socket for %s\n", address->ai_canonname);
            continue;
        }

        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;  // success
        }

        int errsv = errno;
        log_debug("resolve_host: Could not connect to socket: %s\n", strerror(errsv));

        close(socket_fd);
    } while ((address = address->ai_next) != NULL);

    if (address == NULL) {
        return -1;
    }

    return socket_fd;
}

int connect_socket(const char *host, unsigned short port_num) {
    assert(host != NULL);
    assert(port_num > 0);

    log_info("connect_socket: opening connection to %s:%d", host, port_num);

    char port[6];
    sprintf(port, "%d", port_num);

    struct addrinfo hints;
    configure_connect_hints(&hints);

    struct addrinfo* address;

    if (getaddrinfo(host, port, &hints, &address)) {
        int errsv = errno;
        log_error("connect_socket: Failed to get address info for %s: %s", host, gai_strerror(errsv));
        return -1;
    }

    log_info("connect_socket: getaddrinfo returned for %s:%d", host, port_num);

    int socket_fd = resolve_host(address);
    if (socket_fd == -1) {
        log_error("connect_socket: Could not establish a connection to %s", host);
    }
    freeaddrinfo(address);

    return socket_fd;
}

static void configure_listen_hints(struct addrinfo *hints) {
    memset (hints, 0, sizeof (struct addrinfo));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_PASSIVE;
}

int listen_socket(const char *host, unsigned short port_num) {
    assert(host != NULL);
    assert(port_num > 0);

    log_info("listen_socket: attempting to listen on %s:%d", host, port_num);

    char port[6];
    sprintf(port, "%d", port_num);

    struct addrinfo hints;
    configure_listen_hints(&hints);

    struct addrinfo *address;

    if (getaddrinfo (host, port, &hints, &address)) {
        log_error("listen_socket: Unable to getaddrinfo() because of %s", strerror (errno));
        return -1;
    }

    struct addrinfo *current_address;

    for (current_address = address; current_address != NULL; current_address = current_address->ai_next) {

        int listen_fd = socket(current_address->ai_family, current_address->ai_socktype, current_address->ai_protocol);
        if (listen_fd == -1) {
            log_error("listen_socket: socket error: %s", strerror(errno));
            continue;
        }

        if (bind(listen_fd, current_address->ai_addr, current_address->ai_addrlen)) {
            log_error("listen_socket: bind error: %s", strerror (errno));
            close(listen_fd);
            continue;
        }

        if (listen(listen_fd, 1024)) {
            log_error("listen_socket: listen error: %s", strerror(errno));
            close(listen_fd);
            continue;
        }

        log_info("listen_socket: listening on fd [%d]", listen_fd);
        freeaddrinfo(address);
        return listen_fd;
    }

    log_error("listen_socket: unable to listen on specified address.");
    freeaddrinfo(address);
    return -1;
}
