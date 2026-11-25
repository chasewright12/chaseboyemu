
#ifndef PPU_FETCHER_H
#define PPU_FETCHER_H

#include <stdint.h>

#include "fifo.h"
#include "../mem.h"

typedef enum
{
    FETCH_TILE = 0,
    FETCH_DATA_LOW,
    FETCH_DATA_HIGH,
    FETCH_PUSH

} FetcherState;

typedef struct
{
    FetcherState state;

    int phase;
    int window;

    uint8_t tile_x;
    uint8_t source_y;

    uint8_t tile_number;
    uint8_t tile_attr;

    uint8_t tile_bank;
    uint8_t tile_row;

    uint8_t data_low;
    uint8_t data_high;

    uint16_t tile_data_addr;
    uint16_t tile_map_addr;

} PpuFetcher;

/* Initialize fetcher */
void ppu_fetcher_init(
    PpuFetcher *fetcher,
    uint8_t source_x,
    uint8_t source_y,
    int window);

/* Reset fetcher */
void ppu_fetcher_reset(PpuFetcher *fetcher);

/* Advance fetcher by one PPU dot */
void ppu_fetcher_tick(
    PpuFetcher *fetcher,
    Mem *mem,
    PpuFifo *bg_fifo);

#endif
