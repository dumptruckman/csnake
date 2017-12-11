/**
 * Author: Jeremy Wood
 */
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
#include "snake.h"

static volatile bool running = true;

static int parent_pid;
static int child_pid;

static WINDOW *main_window;

static GSList *players = NULL;

static void exit_handler(int dummy) {
    log_info("exit_handler: SIGUSR1 received");
    running = false;
}

// Draw a snake to the screen
static void draw_snake(snake_t *snake) {
    mvprintw(snake->y, snake->x, "O");
}

// Draw the game board
static void update_game_board() {
    clear();
    g_slist_foreach(players, (GFunc) draw_snake, NULL);
    refresh();
}

// Checks if the given snake has the given id
static int snake_has_same_id(const snake_t *snake, const uint32_t *player_id) {
    return snake->player_id - *player_id;
}

// Client process for reading messages sent from the server.
static void read_messages(int client_fd) {
    signal(SIGUSR1, exit_handler);

    struct pollfd events;
    events.fd = client_fd;
    events.events = POLL_IN;

    while (running) {
        poll(&events, 1, 50); // Block for 50 ms at most waiting for events on the client's fd.

        if (events.revents & POLLERR) {
            log_error("read_messages: client socket unexpectedly closed.");
            return;
        }
        if (events.revents & POLLHUP) {
            log_error("read_messages: the server has hung up.");
            return;
        }
        if (events.revents & POLLNVAL) {
            log_error("read_messages: client socket is not open.");
            return;
        }

        if (events.revents & POLLIN) { // Data can be read from the fd.
            // Read the message from the server
            message_t message_type;
            void **message_ptr = malloc(sizeof(void *));
            ssize_t read_amount = recv_message(client_fd, &message_type, message_ptr);
            if (read_amount == 0) {
                log_info("read_messages: Server has shut down.");
                break;
            }

            if (message_type == MSG_SNAKE_UPDATE) {
                msg_snake_update *message = (msg_snake_update *) *message_ptr;

                // Find and update an existing snake or add a new one if none found.
                GSList *existing_snake = g_slist_find_custom(players, &(message->snake.player_id),
                                                             (GCompareFunc) snake_has_same_id);
                if (existing_snake) {
                    snake_t *snake = existing_snake[0].data;
                    snake->x = message->snake.x;
                    snake->y = message->snake.y;
                } else {
                    snake_t *snake = malloc(sizeof(snake_t));
                    snake->player_id = message->snake.player_id;
                    snake->x = message->snake.x;
                    snake->y = message->snake.y;
                    players = g_slist_append(players, snake);
                }

                log_info("read_messages: Received snake update for %d", message->snake.player_id);
            } else if (message_type == MSG_CLIENT_DISCONNECT) {
                msg_client_disconnect *message = (msg_client_disconnect *) *message_ptr;

                // Find and remove an existing snake or log error if none found.
                GSList *existing_snake = g_slist_find_custom(players, &(message->player_id),
                                                             (GCompareFunc) snake_has_same_id);
                if (existing_snake) {
                    players = g_slist_remove(players, existing_snake[0].data);
                    log_info("read_messages: Player %d disconnected.", message->player_id);
                } else {
                    log_error("read_messages: Received disconnect from unknown player %d", message->player_id);
                }
            } else {
                log_error("read_messages: Received unknown message type %d", message_type);
            }

            if (*message_ptr) {
                free(*message_ptr);
            }
            free(message_ptr);

            update_game_board();
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
        // Block until a key is pressed or interrupted
        input_key = getch();

        if (input_key < 0) {
            if (errno == EINTR) {
                // Server has shut down
                break;
            }
            log_error("user_input: getch error: %s", strerror(errno));
        } else {
            log_debug("user_input: Read key %d", input_key);
        }

        switch (input_key) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                // Send the key stroke message to the server
                message = malloc(sizeof(msg_client_keypress));
                ((msg_client_keypress *) message)->key_code = (uint32_t) input_key;
                send_message(client_fd, MSG_CLIENT_KEYPRESS, message);
                free(message);
                break;
            case 27: // Escape
                // Send the key stroke message to the server and then terminate client
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

static void initialize_screen() {
    if (DEBUG) {
        filter();
        newterm(NULL, stdin, stdout);
    } else {
        main_window = initscr();
        noecho();
        curs_set(FALSE);
    }
    keypad(stdscr, TRUE);
}

static void finalize_screen() {
    if (DEBUG) {
        endwin();
    } else {
        endwin();
    }
}

void run_client(char *host, unsigned short port_num) {
    int client_fd = connect_socket(host, port_num);
    if (client_fd < 0) {
        log_error("run_client: Could not connect to server %s:%d", host, port_num);
        return;
    }

    initialize_screen();

    parent_pid = getpid();

    child_pid = fork();
    if (child_pid == 0) {
        // A child process reads messages from the server
        read_messages(client_fd);
    } else {
        // The parent process reads input from the user and sends messages to the server.
        user_input(client_fd);
        int status;
        if (wait(&status) < 0) {
            log_error("run_client: wait error: %s", strerror(errno));
        }
        finalize_screen();
    }

    close(client_fd);
}
