//
// Created by jeremy on 12/9/17.
//

#include <malloc.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>
#include "messages.h"
#include "log.h"
#include "socket.h"
#include "snake.h"

static unsigned char * serialize_char(unsigned char *buffer, uint8_t value) {
    buffer[0] = value;
    return buffer + 1;
}

static unsigned char * serialize_short(unsigned char *buffer, uint16_t value) {
    buffer[0] = (unsigned char) (value >> 8);
    buffer[1] = (unsigned char) value;
    return buffer + 2;
}

static unsigned char * serialize_int(unsigned char *buffer, uint32_t value) {
    log_debug("serialize_int: Serializing int %d", value);
    buffer[0] = (unsigned char) (value >> 24);
    buffer[1] = (unsigned char) (value >> 16);
    buffer[2] = (unsigned char) (value >> 8);
    buffer[3] = (unsigned char) value;
    return buffer + 4;
}

static size_t get_message_size(message_t message_type) {
    // 1 must be added to the size to accommodate for null terminator.
    switch (message_type) {
        case MSG_SNAKE_UPDATE:
            return sizeof(msg_snake_update) + 1;
        case MSG_CLIENT_KEYPRESS:
            return sizeof(msg_client_keypress) + 1;
        case MSG_CLIENT_DISCONNECT:
            return sizeof(msg_client_disconnect) + 1;
        default:
            log_error("get_message_size: Unknown message type %d", message_type);
            return 0;
    }
}

static unsigned char * serialize_msg_snake_update(unsigned char *buffer, msg_snake_update *message) {
    buffer = serialize_int(buffer, message->snake.player_id);
    buffer = serialize_short(buffer, (uint16_t) message->snake.x);
    buffer = serialize_short(buffer, (uint16_t) message->snake.y);
    return buffer;
}

static unsigned char * serialize_msg_client_keypress(unsigned char *buffer, msg_client_keypress *message) {
    buffer = serialize_int(buffer, message->key_code);
    return buffer;
}

static unsigned char * serialize_msg_client_disconnect(unsigned char *buffer, msg_client_disconnect *message) {
    buffer = serialize_int(buffer, message->player_id);
    return buffer;
}

static unsigned char * serialize_message(size_t *size, message_t message_type, void *message_ptr) {
    *size = get_message_size(message_type) + 1; // Add 1 for the message type header.
    if (*size == 1) {
        return NULL;
    }

    log_debug("serialize_message: Creating message of type %d", message_type);

    unsigned char *buffer = malloc(*size);
    unsigned char *original_buffer = buffer;
    buffer = serialize_char(buffer, message_type);

    log_debug("serialize_message: Serialized message type key as '%c'", original_buffer[0]);

    switch (message_type) {
        case MSG_SNAKE_UPDATE:
            buffer = serialize_msg_snake_update(buffer, (msg_snake_update *) message_ptr);
            break;
        case MSG_CLIENT_KEYPRESS:
            buffer = serialize_msg_client_keypress(buffer, (msg_client_keypress *) message_ptr);
            break;
        case MSG_CLIENT_DISCONNECT:
            buffer = serialize_msg_client_disconnect(buffer, (msg_client_disconnect *) message_ptr);
            break;
        default:
            free(buffer);
            log_error("serialize_message: Impossible message type.");
            return NULL;
    }
    buffer[0] = '\0';

    return original_buffer;
}

static const unsigned char * deserialize_char(const unsigned char *message, uint8_t *value) {
    *value = message[0];
    return message + 1;
}

static const unsigned char * deserialize_short(const unsigned char *message, uint16_t *value) {
    unsigned char *buffer = malloc(sizeof(uint16_t));
    buffer[0] = message[1];
    buffer[1] = message[0];
    *value = *((uint16_t *) buffer);
    free(buffer);
    return message + 2;
}

static const unsigned char * deserialize_int(const unsigned char *message, uint32_t *value) {
    unsigned char *buffer = malloc(sizeof(uint32_t));
    buffer[0] = message[3];
    buffer[1] = message[2];
    buffer[2] = message[1];
    buffer[3] = message[0];
    *value = *((uint32_t *) buffer);
    free(buffer);
    return message + 4;
}

static msg_snake_update * deserialize_msg_snake_update(const unsigned char *message_ptr) {
    msg_snake_update *message = malloc(sizeof(msg_snake_update));
    message_ptr = deserialize_int(message_ptr, &(message->snake.player_id));
    message_ptr = deserialize_short(message_ptr, (uint16_t *) &(message->snake.x));
    deserialize_short(message_ptr, (uint16_t *) &(message->snake.y));
    return message;
}

static msg_client_keypress * deserialize_msg_client_keypress(const unsigned char *message_ptr) {
    msg_client_keypress *message = malloc(sizeof(msg_client_keypress));
    deserialize_int(message_ptr, &(message->key_code));
    return message;
}

static msg_client_disconnect * deserialize_msg_client_disconnect(const unsigned char *message_ptr) {
    msg_client_disconnect *message = malloc(sizeof(msg_client_disconnect));
    deserialize_int(message_ptr, &(message->player_id));
    return message;
}

static void * deserialize_message(message_t message_type, const unsigned char *message_ptr) {
    switch (message_type) {
        case MSG_SNAKE_UPDATE:
            return deserialize_msg_snake_update(message_ptr);
        case MSG_CLIENT_KEYPRESS:
            return deserialize_msg_client_keypress(message_ptr);
        case MSG_CLIENT_DISCONNECT:
            return deserialize_msg_client_disconnect(message_ptr);
        default:
            log_error("deserialize_message: Impossible message type.");
            return NULL;
    }
}

void send_message(int fd, message_t message_type, void *message_ptr) {
    size_t size;
    unsigned char * message = serialize_message(&size, message_type, message_ptr);

    ssend(fd, message, size);
}

ssize_t recv_message(int fd, message_t *message_type, void **message_ptr) {
    ssize_t read_amount = srecv(fd, message_type, 1);

    if (read_amount <= 0) {
        return read_amount;
    }

    log_debug("recv_message: Read message type: %d", *message_type);

    size_t size = get_message_size(*message_type);
    if (size == 0) {
        return 0;
    }

    char *read_buffer = malloc(size);
    read_amount = srecv(fd, read_buffer, size);
    if (read_amount <= 0) {
        return read_amount;
    }

    *message_ptr = deserialize_message(*message_type, (unsigned char *) read_buffer);
    //free(read_buffer);
    return size;
}