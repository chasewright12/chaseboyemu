from test_hdma import run_test

SOURCE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ppu/obj_fifo.h"

static ObjFifo fifo;

static ObjPixel make_pixel(
    int color,
    int oam,
    int x
)
{
    ObjPixel pixel = {0};

    pixel.color_id = color;
    pixel.oam_index = oam;
    pixel.sprite_x = x;

    return pixel;
}

static void test_initialization(void)
{
    obj_fifo_init(&fifo);

    assert(obj_fifo_empty(&fifo));
    assert(!obj_fifo_full(&fifo));
    assert(obj_fifo_size(&fifo) == 0);

    puts("PASS: OBJ FIFO initialization");
}

static void test_push_pop(void)
{
    obj_fifo_init(&fifo);

    for (int i = 0; i < 8; i++)
    {
        assert(obj_fifo_push(
            &fifo,
            make_pixel(i % 4, i, 16)
        ));
    }

    assert(obj_fifo_full(&fifo));

    assert(!obj_fifo_push(
        &fifo,
        make_pixel(1, 0, 0)
    ));

    for (int i = 0; i < 8; i++)
    {
        ObjPixel pixel;

        assert(obj_fifo_pop(&fifo, &pixel));
        assert(pixel.color_id == i % 4);
        assert(pixel.oam_index == i);
    }

    assert(obj_fifo_empty(&fifo));
    assert(!obj_fifo_pop(&fifo, NULL));

    puts("PASS: OBJ FIFO push and pop");
}

static void test_wraparound(void)
{
    obj_fifo_init(&fifo);

    for (int i = 0; i < 8; i++)
        obj_fifo_push(&fifo, make_pixel(1, i, 16));

    for (int i = 0; i < 4; i++)
        obj_fifo_pop(&fifo, NULL);

    for (int i = 8; i < 12; i++)
        assert(obj_fifo_push(
            &fifo,
            make_pixel(1, i, 16)
        ));

    for (int i = 4; i < 12; i++)
    {
        ObjPixel pixel;

        assert(obj_fifo_pop(&fifo, &pixel));
        assert(pixel.oam_index == i);
    }

    puts("PASS: OBJ FIFO circular wraparound");
}

static void test_transparency(void)
{
    obj_fifo_init(&fifo);

    ObjPixel a[8];
    ObjPixel b[8];

    for (int i = 0; i < 8; i++)
    {
        a[i] = make_pixel(0, 0, 16);
        b[i] = make_pixel(2, 1, 16);
    }

    assert(obj_fifo_merge(&fifo, a, 0, 0));
    assert(obj_fifo_merge(&fifo, b, 0, 0));

    for (int i = 0; i < 8; i++)
    {
        ObjPixel pixel;

        assert(obj_fifo_pop(&fifo, &pixel));
        assert(pixel.color_id == 2);
    }

    puts("PASS: OBJ transparency");
}

static void test_cgb_priority(void)
{
    obj_fifo_init(&fifo);

    ObjPixel a[8];
    ObjPixel b[8];

    for (int i = 0; i < 8; i++)
    {
        a[i] = make_pixel(1, 5, 8);
        b[i] = make_pixel(3, 2, 16);
    }

    obj_fifo_merge(&fifo, a, 0, 0);
    obj_fifo_merge(&fifo, b, 0, 0);

    ObjPixel pixel;

    assert(obj_fifo_peek(&fifo, &pixel));
    assert(pixel.color_id == 3);
    assert(pixel.oam_index == 2);

    puts("PASS: CGB OAM priority");
}

static void test_dmg_priority(void)
{
    obj_fifo_init(&fifo);

    ObjPixel a[8];
    ObjPixel b[8];

    for (int i = 0; i < 8; i++)
    {
        a[i] = make_pixel(1, 0, 24);
        b[i] = make_pixel(2, 5, 16);
    }

    obj_fifo_merge(&fifo, a, 0, 1);
    obj_fifo_merge(&fifo, b, 0, 1);

    ObjPixel pixel;

    assert(obj_fifo_peek(&fifo, &pixel));
    assert(pixel.color_id == 2);
    assert(pixel.sprite_x == 16);

    puts("PASS: DMG X-coordinate priority");
}

static void test_equal_x_priority(void)
{
    obj_fifo_init(&fifo);

    ObjPixel a[8];
    ObjPixel b[8];

    for (int i = 0; i < 8; i++)
    {
        a[i] = make_pixel(1, 5, 16);
        b[i] = make_pixel(2, 2, 16);
    }

    obj_fifo_merge(&fifo, a, 0, 1);
    obj_fifo_merge(&fifo, b, 0, 1);

    ObjPixel pixel;

    assert(obj_fifo_peek(&fifo, &pixel));
    assert(pixel.oam_index == 2);

    puts("PASS: DMG equal-X OAM priority");
}

static void test_clipping(void)
{
    obj_fifo_init(&fifo);

    ObjPixel pixels[8];

    for (int i = 0; i < 8; i++)
        pixels[i] = make_pixel(1, i, 4);

    assert(obj_fifo_merge(&fifo, pixels, -4, 0));
    assert(obj_fifo_size(&fifo) == 4);

    ObjPixel pixel;

    assert(obj_fifo_pop(&fifo, &pixel));
    assert(pixel.oam_index == 4);

    puts("PASS: OBJ left-edge clipping");
}

static void test_offset(void)
{
    obj_fifo_init(&fifo);

    ObjPixel pixels[8];

    for (int i = 0; i < 8; i++)
        pixels[i] = make_pixel(3, 0, 16);

    assert(obj_fifo_merge(&fifo, pixels, 3, 0));

    ObjPixel pixel;

    for (int i = 0; i < 3; i++)
    {
        assert(obj_fifo_pop(&fifo, &pixel));
        assert(pixel.color_id == 0);
    }

    assert(obj_fifo_pop(&fifo, &pixel));
    assert(pixel.color_id == 3);

    puts("PASS: OBJ FIFO offset alignment");
}

int main(void)
{
    test_initialization();
    test_push_pop();
    test_wraparound();
    test_transparency();
    test_cgb_priority();
    test_dmg_priority();
    test_equal_x_priority();
    test_clipping();
    test_offset();

    puts("\nAll OBJ FIFO tests passed.");

    return 0;
}
'''

if __name__ == "__main__":
    run_test("test_obj_fifo", SOURCE)
