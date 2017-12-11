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

static char * string_to_hex(const char * string, size_t size) {
    char *result = malloc(size * 2 + 1);
    char *buffer = result;
    for (size_t i = 0; i < size; i ++) {
        sprintf(buffer, "%02X", string[i]);
        buffer += 2;
    }
    buffer[0] = '\0';
    return result;
}

ssize_t ssend(int fd, void *message, size_t size) {
    size_t left = size;
    ssize_t written_amount;

    char *hex = string_to_hex(message, size);
    log_debug("ssend: Sending hex data: %s", hex);
    free(hex);

    do {
        log_debug("ssend: Sending %d bytes", left);
        written_amount = write(fd, message, left);
        if (written_amount < 0) {
            log_error("ssend: write error: %s", strerror(errno));
            return written_amount;
        } else if (written_amount == 0) {
            return written_amount;
        } else {
            message += written_amount;
            left -= written_amount;
        }
    } while (left > 0);
}

ssize_t srecv(int fd, void *buffer, size_t size) {
    unsigned char *read_buffer = (unsigned char *) buffer;
    size_t left = size;
    ssize_t read_amount;
    do {
        log_debug("srecv: Reading %d bytes", left);

        read_amount = read(fd, read_buffer, left);
        if (read_amount < 0) {
            log_error("srecv: read error: %s", strerror(errno));
            return read_amount;
        } else if (read_amount == 0) {
            return read_amount;
        } else {
            read_buffer += read_amount;
            left -= read_amount;
        }
    } while (left > 0);

    char *hex = string_to_hex(buffer, size);
    log_debug("srecv: Received hex data: %s", hex);
    free(hex);

    return size;
}