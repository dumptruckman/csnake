#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <ncurses.h>

#include "client.h"
#include "socket.h"
#include "common.h"
#include "log.h"
#include "messages.h"

#define MAX_CLIENTS 30

static GSList *clients = NULL;
static pthread_mutex_t clients_mutex;
static volatile bool running = true;

static int server_socket;

typedef struct {
    int originator_fd;
    char *message;
} message_data_t;

//static void send_message(gpointer data, gpointer user_data) {
//    // Extract the desired types from the arguments.
//    client_t *client = (client_t *) data;
//    message_data_t *message_data = (message_data_t *) user_data;
//
//    if (message_data->originator_fd == client->client_socket) {
//        // Don't send the message to the client the message came from.
//        log_info("send_message: NOT sending \"%s\" from [%d] to fd [%d]", message_data->message, message_data->originator_fd, client->client_socket);
//        return;
//    }
//
//    log_info("send_message: Sending \"%s\" from [%d] to [%d]", message_data->message, message_data->originator_fd, client->client_socket);
//    write(client->client_socket, message_data->message, strlen(message_data->message));
//}

static void client_signal_handler(int dummy) {
    log_debug("A client received SIGUSR1");
}

int receive_keypress(int client_fd) {
    uint32_t result;
    char *data = (char *) &result;
    size_t left = sizeof(result);
    ssize_t amount_read;

    do {
        amount_read = read(client_fd, data, left);
        if (amount_read <= 0) {
            if (errno == EINTR) {
                // Thread was interrupted by main thread. Continuing will check running to see if shutdown should occur.
                // A signal handler may be necessary for this.
                return 0;
            }
        } else {
            data += amount_read;
            left -= amount_read;
        }
    } while (left > 0);

    return ntohl(result);
}

static void *accept_client(void *client_ptr) {
    // Block SIGINT since the main thread takes care of that.
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

    signal(SIGUSR1, client_signal_handler);

    client_t *client = (client_t *) client_ptr;
    char message[MAX_MESSAGE_SIZE];

    while (running) {
        log_debug("accept_client: Awaiting messages from [%d]", client->client_socket);

        message_t message_type;
        void **message_ptr = malloc(sizeof(void *));
        ssize_t read_amount = recv_message(client->client_socket, &message_type, message_ptr);
        if (read_amount < 0) {
            if (errno == EINTR) {
                // Thread was interrupted by main thread. Continuing will check running to see if shutdown should occur.
                // A signal handler may be necessary for this.
                continue;
            }
        } else if (read_amount == 0) {
            log_info("accept_client: client [%d] disconnected", client->client_socket);
            break;
        }

        if (message_type == MSG_CLIENT_KEYPRESS) {
            //int client_keypress = receive_keypress(client->client_socket);
            msg_client_keypress *keypress_message = (msg_client_keypress *) *message_ptr;

            log_info("accept_client: Received keypress from [%d]: %d", client->client_socket, keypress_message->key_code);

            if (keypress_message->key_code == 27) {
                log_info("accept_client: Client [%d] disconnected", client->client_socket);
                break;
            }
            switch (keypress_message->key_code) {
                case KEY_UP:
                    log_info("accept_client: [%d] pressed up", client->client_socket);
                    client->y++;
                    break;
                case KEY_DOWN:
                    log_info("accept_client: [%d] pressed down", client->client_socket);
                    client->y--;
                    break;
                case KEY_LEFT:
                    log_info("accept_client: [%d] pressed left", client->client_socket);
                    client->x--;
                    break;
                case KEY_RIGHT:
                    log_info("accept_client: [%d] pressed right", client->client_socket);
                    client->x++;
                    break;
                default:
                    break;
            }
        } else {
            log_error("accept_client: Received unknown message type %d", message_type);
        }

        if (*message_ptr) {
            free(*message_ptr);
        }
        free(message_ptr);
    }

    log_info("accept_client: Shutting down client [%d]", client->client_socket);

    pthread_mutex_lock(&clients_mutex);
    // Remove the finished client from the global client list
    clients = g_slist_remove(clients, client);
    pthread_mutex_unlock(&clients_mutex);

    close(client->client_socket);
    free(client);

    return NULL;
}

static void shutdown_client(gpointer data, gpointer dummy) {
    client_t *client = (client_t *) data;

    log_info("shutdown_client: Disconnecting client [%d]", client->client_socket);
    //pthread_kill(client->client_thread, SIGUSR1);
    shutdown(client->client_socket, SHUT_RD);

    // Main thread should wait until client shuts down.
    pthread_join(client->client_thread, NULL);
}

static void interrupt_handler(int dummy) {
    log_info("interrupt_handler: SIGINT received. Shutting down server...");
    running = false;

    log_debug("run_server: Copying client list");
    pthread_mutex_lock(&clients_mutex);
    GSList *clients_copy = g_slist_copy(clients);
    pthread_mutex_unlock(&clients_mutex);

    log_info("run_server: Attempting to shut down all clients");
    // Shutdown the clients using a copy of the client list to avoid deadlock and potential other issues.
    g_slist_foreach(clients_copy, shutdown_client, NULL);

    shutdown(server_socket, SHUT_RDWR);
}

void run_server(char *host, unsigned short port_num) {
    signal(SIGINT, interrupt_handler);

    // Open a socket for listening.
    server_socket = listen_socket(host, port_num);

    if (server_socket == -1) {
        log_error("run_server: Could not open server socket.");
        return;
    }

    struct sockaddr_in client_address;
    socklen_t client_length = sizeof(client_address);

    while (running) {
        log_debug("run_server: Awaiting connections");
        // Block until client connection received.
        int client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_length);
        if (client_socket < 0) {
            log_error("run_server: accept error: %s", strerror(errno));
            continue;
        }

        // Initialize client struct.
        client_t *client = malloc(sizeof(client_t));
        client->client_socket = client_socket;
        client->x = 0;
        client->y = 0;

        // Run the client thread and track the thread in the client struct
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, accept_client, client);
        client->client_thread = client_thread;

        // Add the client to the global client list.
        pthread_mutex_lock(&clients_mutex);

        clients = g_slist_append(clients, client);
        pthread_mutex_unlock(&clients_mutex);

        log_info("run_server: Accepted connection from %s on fd [%d]", inet_ntoa(client_address.sin_addr),
                 client_socket);
    }

    close(server_socket);

    log_info("run_server: Server shutdown complete.");
}
