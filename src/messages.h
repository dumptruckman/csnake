//
// Created by jeremy on 12/9/17.
//

#ifndef CSNAKE_MESSAGES_H
#define CSNAKE_MESSAGES_H

#include <stdint.h>
#include <glib.h>
#include "snake.h"

typedef uint8_t message_t;

typedef struct {
    snake_t snake;
} msg_snake_update;
#define MSG_SNAKE_UPDATE 0

typedef struct {
    uint32_t key_code;
} msg_client_keypress;
#define MSG_CLIENT_KEYPRESS 1

void send_message(int fd, message_t message_type, void *message_ptr);
ssize_t recv_message(int fd, message_t *message_type, void **message_ptr);

#endif //CSNAKE_MESSAGES_H
