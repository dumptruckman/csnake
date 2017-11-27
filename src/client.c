#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>

#include "socket.h"
#include "common.h"
#include "log.h"

bool_t running = true;
int pid;

void exit_handler(int signo) {
    log_info("exit_handler: SIGUSR1 received");
    running = false;
}

void read_responses(int client_fd) {
    signal(SIGUSR1, exit_handler);

    struct pollfd events;
    events.fd = client_fd;
    events.events = POLL_IN;

    char message[MAX_MESSAGE_SIZE];

    while (running) {
        poll(&events, 1, 50);

        if (events.revents & POLLERR) {
            log_error("read_responses: client socket unexpectedly closed.");
            return;
        }
        if (events.revents & POLLHUP) {
            log_error("read_responses: the server has hung up.");
            return;
        }
        if (events.revents & POLLNVAL) {
            log_error("read_responses: client socket is not open.");
            return;
        }

        if (events.revents & POLLIN) {
            read(client_fd, message, MAX_MESSAGE_SIZE);
            printf("Received: %s\n", message);
            fflush(stdout);
        }
    }
}

void user_input(int client_fd) {
    char input[MAX_MESSAGE_SIZE];

    while (scanf("%" STR(MAX_MESSAGE_SIZE) "[^\n]%*c", input) > 0) {
        if (!strcmp(input, "exit")) {
            break;
        }

        size_t amount_to_write = strlen(input);
        ssize_t result = write(client_fd, input, amount_to_write);
        if (result < 0) {
            log_error("user_input: write error: %s", strerror(errno));
        } else if (result != amount_to_write) {
            log_error("user_input: Could not write full message to socket");
            char sent_message[amount_to_write];
            strncpy(sent_message, input, (size_t) result);
            printf("Sent: %s\n", sent_message);
            fflush(stdout);
        } else {
            printf("Sent: %s\n", input);
            fflush(stdout);
        }
    }

    kill(pid, SIGUSR1);
}

void run_client(char *host, unsigned short port_num) {
    int client_fd = connect_socket(host, port_num);

    pid = fork();
    if (pid == 0) {
        read_responses(client_fd);
    } else {
        user_input(client_fd);
        int status;
        if (wait(&status) < 0) {
            log_error("run_client: wait error: %s", strerror(errno));
        }
    }

    close(client_fd);
}
