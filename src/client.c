#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ncurses.h>
#include <netinet/in.h>

#include "socket.h"
#include "common.h"
#include "log.h"
#include "messages.h"

static volatile bool running = true;

static int parent_pid;
static int child_pid;

static WINDOW *main_window;

static void exit_handler(int dummy) {
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

static void child_handler(int dummy) { }

static void user_input(int client_fd) {
    // Set up a SIGCHLD handler that will not resume the interrupted action.
    struct sigaction signal_action;
    signal_action.sa_handler = child_handler;
    signal_action.sa_flags = 0;
    sigemptyset(&signal_action.sa_mask);
    sigaction(SIGCHLD, &signal_action, NULL);

    int input_key;
    void *message;

    while (running) {
        clear();
        input_key = getch();

        if (input_key < 0) {
            if (errno == EINTR) {
                // Server shut down
                break;
            }
        }

        log_debug("user_input: Read key %d", input_key);

        switch (input_key) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                message = malloc(sizeof(msg_client_keypress));
                ((msg_client_keypress *) message)->key_code = (uint32_t) input_key;
                send_message(client_fd, MSG_CLIENT_KEYPRESS, message);
                free(message);
                break;
            case 27: // Escape - quit program
                message = malloc(sizeof(msg_client_keypress));
                ((msg_client_keypress *) message)->key_code = (uint32_t) input_key;
                send_message(client_fd, MSG_CLIENT_KEYPRESS, message);
                free(message);
                kill(child_pid, SIGUSR1);
                return;
            default:
                break;
        }
    }
}

void run_client(char *host, unsigned short port_num) {
    int client_fd = connect_socket(host, port_num);
    if (client_fd < 0) {
        log_error("run_client: Could not connect to server %s:%d", host, port_num);
        return;
    }

    filter();
    newterm(NULL, stdin, stdout);
    keypad(stdscr, TRUE);
    //main_window = initscr();
    //noecho();
    //curs_set(FALSE);

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
        endwin();
    }

    close(client_fd);
}
