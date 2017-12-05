#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>

#include "socket.h"
#include "common.h"
#include "log.h"

static volatile bool running = true;

static int parent_pid;
static int child_pid;

static void exit_handler(int signo) {
    log_info("exit_handler: SIGUSR1 received");
    running = false;
}

static void read_responses(int client_fd) {
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
            ssize_t read_amount = read(client_fd, message, MAX_MESSAGE_SIZE);
            if (read_amount == 0) {
                log_info("read_responses: Server has shut down.");
                break;
            }
            message[read_amount] = '\0';
            printf("Received: %s\n", message);
            fflush(stdout);
        }
    }
}

static void child_handler(int dummy) {

}

static void user_input(int client_fd) {
    // Set up a SIGCHLD handler that will not resume the interrupted action.
    struct sigaction signal_action;
    signal_action.sa_handler = child_handler;
    signal_action.sa_flags = 0;
    sigemptyset(&signal_action.sa_mask);
    sigaction(SIGCHLD, &signal_action, NULL);


    char input[MAX_MESSAGE_SIZE];
    input[0] = '\0';

    // This pattern looks funny but it is correct
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
            char *sent_message = malloc(amount_to_write + 1);
            strncpy(sent_message, input, (size_t) amount_to_write);
            printf("Sent: %s\n", sent_message);
            fflush(stdout);
            free(sent_message);
        } else {
            printf("Sent: %s\n", input);
            fflush(stdout);
        }
        input[0] = '\0';
    }

    kill(child_pid, SIGUSR1);
}

void run_client(char *host, unsigned short port_num) {
    int client_fd = connect_socket(host, port_num);

    parent_pid = getpid();

    child_pid = fork();
    if (child_pid == 0) {
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
