#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>
#include <arpa/inet.h>

#include "client.h"
#include "socket.h"
#include "common.h"
#include "log.h"

#define MAX_CLIENTS 30

GSList *clients = NULL;

typedef struct {
    int originator_fd;
    char *message;
} message_data_t;

static void send_message(gpointer data, gpointer user_data) {
    client_t *client = (client_t *) data;
    message_data_t *message_data = (message_data_t *) user_data;

    if (message_data->originator_fd == client->client_socket) {
        // Don't send the message to the client the message came from.
        return;
    }

    write(client->client_socket, message_data->message, strlen(message_data->message));
}

static void *accept_client(void *client_socket_ptr) {
    int client_socket = *((int *) client_socket_ptr);

    while (true) {
        char message[MAX_MESSAGE_SIZE];

        if (read(client_socket, message, MAX_MESSAGE_SIZE) < 0) {
            log_error("accept_client: read error: %s", strerror(errno));
        }

        printf("Received: %s\n", message);
        fflush(stdout);

        message_data_t message_data;
        message_data.message = message;
        message_data.originator_fd = client_socket;

        // Forward the message to each of the other clients
        g_slist_foreach(clients, send_message, (gpointer) &message_data);
    }

    close(client_socket);


    return NULL;
}

void run_server(char *host, unsigned short port_num) {
    int server_socket = listen_socket(host, port_num);

    struct sockaddr_in client_address;
    socklen_t client_length = sizeof(client_address);

    while (true) {
        int client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_length);
        if (client_socket < 0) {
            log_error("run_server: accept error: %s", strerror(errno));
            continue;
        }

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, accept_client, &client_socket);

        client_t client;
        client.client_socket = client_socket;
        client.client_thread = client_thread;

        clients = g_slist_append(clients, &client);
        log_info("run_server: Accepted connection from %s on fd [%d]", inet_ntoa(client_address.sin_addr),
                 client_socket);
    }

    close(server_socket);
}
