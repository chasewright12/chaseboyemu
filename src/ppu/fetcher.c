
#include "fetcher.h"

#include <string.h>

/* LCDC register */
#define REG_LCDC 0x40

/* LCDC flags */
#define LCDC_BG_MAP 0x08
#define LCDC_TILE_DATA 0x10
#define LCDC_WIN_MAP 0x40

/* Initialize fetcher */
void ppu_fetcher_init(
    PpuFetcher *fetcher,
    uint8_t source_x,
    uint8_t source_y,
    int window)
{
    if (!fetcher)
        return;

    memset(fetcher, 0, sizeof(*fetcher));

    fetcher->state = FETCH_TILE;
    fetcher->phase = 0;

    fetcher->window = window;

    /*
     * Start from the beginning of the tile.
     *
     * Background scrolling may begin in the middle
     * of a tile, so source_x is aligned to 8 pixels.
     */
    fetcher->tile_x = source_x & 0xF8;

    fetcher->source_y = source_y;
}

/* Reset fetcher */
void ppu_fetcher_reset(PpuFetcher *fetcher)
{
    if (!fetcher)
        return;

    fetcher->state = FETCH_TILE;
    fetcher->phase = 0;

    fetcher->tile_number = 0;
    fetcher->tile_attr = 0;

    fetcher->tile_bank = 0;
    fetcher->tile_row = 0;

    fetcher->data_low = 0;
    fetcher->data_high = 0;

    fetcher->tile_data_addr = 0;
    fetcher->tile_map_addr = 0;
}

/*
 * Fetch tile number and CGB attributes.
 */
static void fetch_tile(
    PpuFetcher *fetcher,
    Mem *mem)
{
    uint8_t lcdc = mem->io[REG_LCDC];

    int map_base;

    if (fetcher->window)
    {
        map_base = (lcdc & LCDC_WIN_MAP)
                       ? 0x1C00
                       : 0x1800;
    }
    else
    {
        map_base = (lcdc & LCDC_BG_MAP)
                       ? 0x1C00
                       : 0x1800;
    }

    int map_x = (fetcher->tile_x >> 3) & 31;
    int map_y = (fetcher->source_y >> 3) & 31;

    fetcher->tile_map_addr =
        (uint16_t)(map_base + map_y * 32 + map_x);

    /*
     * Bank 0 contains tile numbers.
     * Bank 1 contains CGB tile attributes.
     */
    fetcher->tile_number =
        mem->vram[0][fetcher->tile_map_addr];

    fetcher->tile_attr = mem->cgb
                             ? mem->vram[1][fetcher->tile_map_addr]
                             : 0;

    /*
     * CGB tile attributes:
     *
     * Bits 0-2: palette
     * Bit 3: VRAM bank
     * Bit 5: horizontal flip
     * Bit 6: vertical flip
     * Bit 7: BG priority
     */
    fetcher->tile_bank =
        (fetcher->tile_attr >> 3) & 1;

    int row = fetcher->source_y & 7;

    if (fetcher->tile_attr & 0x40)
        row = 7 - row;

    fetcher->tile_row = (uint8_t)row;

    /*
     * Tile data addressing:
     *
     * LCDC bit 4 = 1:
     * 0x8000 - 0x8FFF (unsigned tile numbers)
     *
     * LCDC bit 4 = 0:
     * 0x8800 - 0x97FF (signed tile numbers)
     *
     * VRAM addresses here are offsets into
     * the selected 0x2000-byte VRAM bank.
     */
    int tile_offset;

    if (lcdc & LCDC_TILE_DATA)
    {
        tile_offset = fetcher->tile_number * 16;
    }
    else
    {
        tile_offset =
            0x1000 + (int8_t)fetcher->tile_number * 16;
    }

    fetcher->tile_data_addr =
        (uint16_t)(tile_offset + fetcher->tile_row * 2);
}

/* Fetch lower tile data byte */
static void fetch_data_low(
    PpuFetcher *fetcher,
    Mem *mem)
{
    fetcher->data_low =
        mem->vram[fetcher->tile_bank]
                 [fetcher->tile_data_addr];
}

/* Fetch upper tile data byte */
static void fetch_data_high(
    PpuFetcher *fetcher,
    Mem *mem)
{
    fetcher->data_high =
        mem->vram[fetcher->tile_bank]
                 [fetcher->tile_data_addr + 1];
}

/*
 * Convert two bitplanes into eight pixels
 * and push them into the background FIFO.
 */
static int fetch_push(
    PpuFetcher *fetcher,
    PpuFifo *bg_fifo)
{
    /*
     * Do not push another tile if more than
     * eight pixels are waiting in the FIFO.
     */
    if (ppu_fifo_size(bg_fifo) > 8)
        return 0;

    for (int px = 0; px < 8; px++)
    {
        int bit = 7 - px;

        /*
         * CGB horizontal flip.
         */
        if (fetcher->tile_attr & 0x20)
            bit = px;

        uint8_t low =
            (fetcher->data_low >> bit) & 1;

        uint8_t high =
            (fetcher->data_high >> bit) & 1;

        PpuPixel pixel = {0};

        pixel.color_id =
            (uint8_t)((high << 1) | low);

        pixel.palette =
            fetcher->tile_attr & 0x07;

        pixel.priority =
            (fetcher->tile_attr >> 7) & 1;

        pixel.bank =
            fetcher->tile_bank;

        pixel.is_obj = 0;
        pixel.behind_bg = 0;
        pixel.oam_index = 0;

        if (!ppu_fifo_push(bg_fifo, pixel))
            return 0;
    }

    /*
     * Advance to the next tile.
     */
    fetcher->tile_x += 8;

    return 1;
}

/*
 * Advance the Pixel Fetcher by one PPU dot.
 *
 * Each fetch stage normally consumes two dots.
 * PUSH waits until the FIFO has enough free space.
 */
void ppu_fetcher_tick(
    PpuFetcher *fetcher,
    Mem *mem,
    PpuFifo *bg_fifo)
{
    if (!fetcher || !mem || !bg_fifo)
        return;

    fetcher->phase++;

    if (fetcher->phase < 2)
        return;

    /*
     * PUSH can stall independently.
     */
    if (fetcher->state == FETCH_PUSH)
    {
        if (!fetch_push(fetcher, bg_fifo))
        {
            fetcher->phase = 1;
            return;
        }

        fetcher->state = FETCH_TILE;
        fetcher->phase = 0;

        return;
    }

    fetcher->phase = 0;

    switch (fetcher->state)
    {
    case FETCH_TILE:

        fetch_tile(fetcher, mem);

        fetcher->state = FETCH_DATA_LOW;

        break;

    case FETCH_DATA_LOW:

        fetch_data_low(fetcher, mem);

        fetcher->state = FETCH_DATA_HIGH;

        break;

    case FETCH_DATA_HIGH:

        fetch_data_high(fetcher, mem);

        fetcher->state = FETCH_PUSH;

        break;

    case FETCH_PUSH:

        /*
         * PUSH is handled above.
         */
        break;
    }
}
