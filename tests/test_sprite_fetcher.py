from test_hdma import run_test

SOURCE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "ppu/sprite_fetcher.h"

static Mem mem;
static SpriteFetcher fetcher;

static void setup(void)
{
    memset(&mem, 0, sizeof(mem));

    mem.cgb = 1;
    mem.io[0x40] = 0x93;

    sprite_fetcher_init(&fetcher);
}

static SpriteEntry make_sprite(
    int tile,
    int flags
)
{
    SpriteEntry sprite = {
        .y = 16,
        .x = 8,
        .tile = tile,
        .flags = flags,
        .oam_index = 0
    };

    return sprite;
}

static void fetch(
    SpriteEntry sprite,
    int ly,
    ObjPixel pixels[8]
)
{
    assert(sprite_fetcher_start(
        &fetcher,
        &mem,
        &sprite,
        ly
    ));

    for (int i = 0; i < 6; i++)
        sprite_fetcher_tick(&fetcher, &mem);

    assert(sprite_fetcher_ready(&fetcher));

    assert(sprite_fetcher_take(
        &fetcher,
        pixels
    ));
}

static void test_initialization(void)
{
    setup();

    assert(!sprite_fetcher_busy(&fetcher));
    assert(!sprite_fetcher_ready(&fetcher));

    assert(fetcher.state == SPRITE_FETCH_IDLE);

    puts("PASS: sprite fetcher initialization");
}

static void test_oam_selection(void)
{
    setup();

    for (int i = 0; i < 40; i++)
    {
        mem.oam[i * 4] = 16;
        mem.oam[i * 4 + 1] = 8 + i;
        mem.oam[i * 4 + 2] = i;
    }

    SpriteEntry sprites[10];

    int count = sprite_fetcher_select(
        &mem,
        0,
        sprites
    );

    assert(count == 10);

    for (int i = 0; i < 10; i++)
        assert(sprites[i].oam_index == i);

    puts("PASS: ten-sprite scanline limit");
}

static void test_scanline_filter(void)
{
    setup();

    mem.oam[0] = 16;
    mem.oam[4] = 24;
    mem.oam[8] = 32;

    SpriteEntry sprites[10];

    assert(sprite_fetcher_select(
        &mem, 8, sprites
    ) == 1);

    assert(sprites[0].oam_index == 1);

    puts("PASS: OAM scanline filtering");
}

static void test_pixel_decoding(void)
{
    setup();

    mem.vram[0][0] = 0xAA;
    mem.vram[0][1] = 0xCC;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0), 0, pixels);

    const int expected[8] = {
        3, 2, 1, 0, 3, 2, 1, 0
    };

    for (int i = 0; i < 8; i++)
        assert(pixels[i].color_id == expected[i]);

    puts("PASS: sprite 2BPP pixel decoding");
}

static void test_x_flip(void)
{
    setup();

    mem.vram[0][0] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x20), 0, pixels);

    assert(pixels[0].color_id == 0);
    assert(pixels[7].color_id == 1);

    puts("PASS: sprite X flip");
}

static void test_y_flip(void)
{
    setup();

    mem.vram[0][14] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x40), 0, pixels);

    assert(pixels[0].color_id == 1);

    puts("PASS: sprite Y flip");
}

static void test_vram_bank(void)
{
    setup();

    mem.vram[0][0] = 0x00;
    mem.vram[1][0] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x08), 0, pixels);

    assert(pixels[0].color_id == 1);

    puts("PASS: CGB sprite VRAM bank");
}

static void test_cgb_palette(void)
{
    setup();

    mem.vram[0][0] = 0xFF;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x05), 0, pixels);

    for (int i = 0; i < 8; i++)
        assert(pixels[i].palette == 5);

    puts("PASS: CGB sprite palette");
}

static void test_dmg_palette(void)
{
    setup();

    mem.cgb = 0;
    mem.vram[0][0] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x10), 0, pixels);

    assert(pixels[0].palette == 1);

    puts("PASS: DMG sprite palette");
}

static void test_priority_flag(void)
{
    setup();

    ObjPixel pixels[8];

    fetch(make_sprite(0, 0x80), 0, pixels);

    for (int i = 0; i < 8; i++)
        assert(pixels[i].priority == 1);

    puts("PASS: OBJ background priority flag");
}

static void test_8x16_sprite(void)
{
    setup();

    mem.io[0x40] |= 0x04;

    /*
     * Tile 3 must be treated as tile 2.
     */
    mem.vram[0][2 * 16] = 0x80;
    mem.vram[0][3 * 16 + 1] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(3, 0), 0, pixels);

    assert(pixels[0].color_id == 1);

    fetch(make_sprite(3, 0), 8, pixels);

    assert(pixels[0].color_id == 2);

    puts("PASS: 8x16 sprite tile addressing");
}

static void test_8x16_y_flip(void)
{
    setup();

    mem.io[0x40] |= 0x04;

    mem.vram[0][3 * 16 + 14] = 0x80;

    ObjPixel pixels[8];

    fetch(make_sprite(3, 0x40), 0, pixels);

    assert(pixels[0].color_id == 1);

    puts("PASS: 8x16 sprite Y flip");
}

static void test_fetch_timing(void)
{
    setup();

    SpriteEntry sprite = make_sprite(0, 0);

    assert(sprite_fetcher_start(
        &fetcher,
        &mem,
        &sprite,
        0
    ));

    for (int i = 0; i < 5; i++)
    {
        sprite_fetcher_tick(&fetcher, &mem);

        assert(sprite_fetcher_busy(&fetcher));
        assert(!sprite_fetcher_ready(&fetcher));
    }

    sprite_fetcher_tick(&fetcher, &mem);

    assert(!sprite_fetcher_busy(&fetcher));
    assert(sprite_fetcher_ready(&fetcher));

    puts("PASS: six-dot sprite fetch timing");
}

int main(void)
{
    test_initialization();
    test_oam_selection();
    test_scanline_filter();
    test_pixel_decoding();
    test_x_flip();
    test_y_flip();
    test_vram_bank();
    test_cgb_palette();
    test_dmg_palette();
    test_priority_flag();
    test_8x16_sprite();
    test_8x16_y_flip();
    test_fetch_timing();

    puts("\nAll sprite fetcher tests passed.");

    return 0;
}
'''

if __name__ == "__main__":
    run_test("test_sprite_fetcher", SOURCE)
