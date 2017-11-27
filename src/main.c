#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#include "common.h"
#include "log.h"
#include "server.h"
#include "client.h"

int main(int argc, char **argv) {
    bool_t server_mode = false;

    int c;
    while ((c = getopt(argc, argv, "s")) != -1) {
        switch (c) {
            case 's':
                server_mode = true;
                break;
            default:
                exit(0);
        }
    }

    if (optind + 1 >= argc) {
        log_error("Usage is %s [-s] <host> <port>\n", argv[0]);
        exit(0);
    }

    char *host = argv[optind];
    char *port = argv[optind + 1];

    char *ptr;
    long int port_num = strtol(port, &ptr, 10);
    if (port_num <= 0 || port_num >= 65535) {
        log_error("%li is not a valid port number\n", port_num);
        exit(0);
    }

    if (server_mode) {
        run_server(host, port_num);
    } else {
        run_client(host, port_num);
    }

    return 0;
}
