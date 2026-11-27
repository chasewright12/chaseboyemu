
#ifndef SPRITE_FETCHER_H
#define SPRITE_FETCHER_H

#include <stdint.h>
#include "../mem.h"
#include "obj_fifo.h"

#define SPRITE_WIDTH 8
#define SPRITE_MAX_PER_LINE 10
#define SPRITE_FETCH_DOTS 6

typedef enum
{
    SPRITE_FETCH_IDLE = 0,
    SPRITE_FETCH_TILE,
    SPRITE_FETCH_DATA_LOW,
    SPRITE_FETCH_DATA_HIGH

} SpriteFetchState;

typedef struct
{
    uint8_t y;
    uint8_t x;
    uint8_t tile;
    uint8_t flags;
    uint8_t oam_index;

} SpriteEntry;

typedef struct
{
    SpriteFetchState state;

    SpriteEntry sprite;

    uint8_t active;
    uint8_t ready;
    uint8_t dots;

    uint8_t row;
    uint8_t bank;

    uint16_t tile_address;

    uint8_t data_low;
    uint8_t data_high;

    ObjPixel pixels[SPRITE_WIDTH];

} SpriteFetcher;

void sprite_fetcher_init(SpriteFetcher *fetcher);
void sprite_fetcher_reset(SpriteFetcher *fetcher);

int sprite_fetcher_select(
    const Mem *mem,
    int ly,
    SpriteEntry sprites[SPRITE_MAX_PER_LINE]);

int sprite_fetcher_start(
    SpriteFetcher *fetcher,
    const Mem *mem,
    const SpriteEntry *sprite,
    int ly);

void sprite_fetcher_tick(
    SpriteFetcher *fetcher,
    const Mem *mem);

int sprite_fetcher_busy(
    const SpriteFetcher *fetcher);

int sprite_fetcher_ready(
    const SpriteFetcher *fetcher);

int sprite_fetcher_take(
    SpriteFetcher *fetcher,
    ObjPixel pixels[SPRITE_WIDTH]);

#endif
