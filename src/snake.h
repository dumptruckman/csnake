/**
 * Author: Jeremy Wood
 */

#ifndef CSNAKE_SNAKE_H
#define CSNAKE_SNAKE_H

#include <stdint.h>

typedef struct {
    uint32_t player_id;
    int16_t x, y;
} snake_t;

#endif //CSNAKE_SNAKE_H
