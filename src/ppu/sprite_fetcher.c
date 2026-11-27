
#include "sprite_fetcher.h"
#include <string.h>

#define REG_LCDC 0x40

#define LCDC_OBJ_16 0x04

#define OBJ_FLAG_PRIORITY 0x80
#define OBJ_FLAG_Y_FLIP 0x40
#define OBJ_FLAG_X_FLIP 0x20
#define OBJ_FLAG_DMG_PAL 0x10
#define OBJ_FLAG_VRAM_BANK 0x08

void sprite_fetcher_init(SpriteFetcher *fetcher)
{
    memset(fetcher, 0, sizeof(*fetcher));

    fetcher->state = SPRITE_FETCH_IDLE;
}

void sprite_fetcher_reset(SpriteFetcher *fetcher)
{
    sprite_fetcher_init(fetcher);
}

/*
 * Select the first ten sprites intersecting
 * the current scanline in OAM order.
 */
int sprite_fetcher_select(
    const Mem *mem,
    int ly,
    SpriteEntry sprites[SPRITE_MAX_PER_LINE])
{
    if (ly < 0 || ly >= 144)
        return 0;

    int height =
        (mem->io[REG_LCDC] & LCDC_OBJ_16)
            ? 16
            : 8;

    int count = 0;

    for (int i = 0;
         i < 40 && count < SPRITE_MAX_PER_LINE;
         i++)
    {
        const uint8_t *oam = &mem->oam[i * 4];

        int y = (int)oam[0] - 16;

        if (ly < y || ly >= y + height)
            continue;

        sprites[count].y = oam[0];
        sprites[count].x = oam[1];
        sprites[count].tile = oam[2];
        sprites[count].flags = oam[3];
        sprites[count].oam_index = (uint8_t)i;

        count++;
    }

    return count;
}

// Begin fetching a sprite row.
int sprite_fetcher_start(
    SpriteFetcher *fetcher,
    const Mem *mem,
    const SpriteEntry *sprite,
    int ly)
{
    if (fetcher->active || fetcher->ready)
        return 0;

    int height =
        (mem->io[REG_LCDC] & LCDC_OBJ_16)
            ? 16
            : 8;

    int y = (int)sprite->y - 16;
    int row = ly - y;

    if (row < 0 || row >= height)
        return 0;

    sprite_fetcher_reset(fetcher);

    fetcher->sprite = *sprite;

    if (sprite->flags & OBJ_FLAG_Y_FLIP)
        row = height - 1 - row;

    uint8_t tile = sprite->tile;

    // 8x16 sprites use an even tile index.
    if (height == 16)
        tile &= 0xFE;

    fetcher->row = (uint8_t)row;

    fetcher->bank =
        mem->cgb
            ? ((sprite->flags &
                OBJ_FLAG_VRAM_BANK) != 0)
            : 0;

    fetcher->tile_address =
        (uint16_t)(tile * 16 + row * 2);

    fetcher->state = SPRITE_FETCH_TILE;

    fetcher->active = 1;
    fetcher->ready = 0;
    fetcher->dots = 0;

    return 1;
}

/*
 * Decode the two tile data bytes into
 * eight OBJ pixels.
 */
static void sprite_fetcher_decode(
    SpriteFetcher *fetcher,
    const Mem *mem)
{
    uint8_t flags = fetcher->sprite.flags;

    uint8_t palette =
        mem->cgb
            ? (flags & 0x07)
            : ((flags & OBJ_FLAG_DMG_PAL) != 0);

    for (int i = 0; i < SPRITE_WIDTH; i++)
    {
        int bit =
            (flags & OBJ_FLAG_X_FLIP)
                ? i
                : 7 - i;

        ObjPixel *pixel =
            &fetcher->pixels[i];

        pixel->color_id = (uint8_t)((((fetcher->data_high >> bit) & 1) << 1) |
                                    ((fetcher->data_low >> bit) & 1));

        pixel->palette = palette;

        pixel->priority =
            (flags & OBJ_FLAG_PRIORITY) != 0;

        pixel->oam_index =
            fetcher->sprite.oam_index;

        pixel->sprite_x =
            fetcher->sprite.x;
    }
}

/*
 * Advance the sprite fetcher by one PPU dot.
 *
 * Each fetch stage takes two dots.
 */
void sprite_fetcher_tick(
    SpriteFetcher *fetcher,
    const Mem *mem)
{
    if (!fetcher->active)
        return;

    fetcher->dots++;

    if (fetcher->dots < 2)
        return;

    fetcher->dots = 0;

    switch (fetcher->state)
    {
    case SPRITE_FETCH_TILE:

        fetcher->state =
            SPRITE_FETCH_DATA_LOW;

        break;

    case SPRITE_FETCH_DATA_LOW:

        fetcher->data_low =
            mem->vram[fetcher->bank]
                     [fetcher->tile_address];

        fetcher->state =
            SPRITE_FETCH_DATA_HIGH;

        break;

    case SPRITE_FETCH_DATA_HIGH:

        fetcher->data_high =
            mem->vram[fetcher->bank]
                     [fetcher->tile_address + 1];

        sprite_fetcher_decode(fetcher, mem);

        fetcher->active = 0;
        fetcher->ready = 1;

        fetcher->state =
            SPRITE_FETCH_IDLE;

        break;

    case SPRITE_FETCH_IDLE:
    default:
        break;
    }
}

int sprite_fetcher_busy(
    const SpriteFetcher *fetcher)
{
    return fetcher->active;
}

int sprite_fetcher_ready(
    const SpriteFetcher *fetcher)
{
    return fetcher->ready;
}

int sprite_fetcher_take(
    SpriteFetcher *fetcher,
    ObjPixel pixels[SPRITE_WIDTH])
{
    if (!fetcher->ready)
        return 0;

    memcpy(
        pixels,
        fetcher->pixels,
        sizeof(fetcher->pixels));

    fetcher->ready = 0;

    return 1;
}
