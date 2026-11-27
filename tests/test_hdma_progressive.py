from test_hdma import run_test

SOURCE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "dma/hdma.h"

static Mem m;
static Hdma h;

static void setup(void)
{
    memset(&m, 0, sizeof(m));

    m.cgb = 1;
    m.io[0x40] = 0x91;

    hdma_init(&h, &m);

    m.hdma = &h;

    for (int i = 0; i < 64; i++)
    {
        mem_write(
            &m,
            0xC000 + i,
            (uint8_t)(i + 1)
        );
    }
}

static void start(uint8_t control)
{
    mem_write(&m, 0xFF51, 0xC0);
    mem_write(&m, 0xFF52, 0x00);
    mem_write(&m, 0xFF53, 0x00);
    mem_write(&m, 0xFF54, 0x00);
    mem_write(&m, 0xFF55, control);
}

static void test_progressive_gdma(void)
{
    setup();

    start(0x00);

    assert(hdma_is_active(&h));
    assert(hdma_cpu_stalled(&h));

    assert(m.vram[0][0] == 0);

    hdma_tick(&h, 1);

    assert(m.vram[0][0] == 0);

    hdma_tick(&h, 1);

    assert(m.vram[0][0] == 1);
    assert(m.vram[0][1] == 0);

    hdma_tick(&h, 14);

    for (int i = 0; i < 8; i++)
        assert(m.vram[0][i] == i + 1);

    assert(m.vram[0][8] == 0);

    hdma_tick(&h, 16);

    for (int i = 0; i < 16; i++)
        assert(m.vram[0][i] == i + 1);

    assert(!hdma_is_active(&h));
    assert(!hdma_cpu_stalled(&h));

    assert(mem_read(&m, 0xFF55) == 0xFF);

    puts("PASS: progressive GDMA");
}

static void test_multiple_blocks(void)
{
    setup();

    start(0x01);

    hdma_tick(&h, 32);

    assert(hdma_is_active(&h));
    assert(hdma_cpu_stalled(&h));

    assert(m.vram[0][15] == 16);
    assert(m.vram[0][16] == 0);

    hdma_tick(&h, 32);

    assert(m.vram[0][31] == 32);

    assert(!hdma_is_active(&h));
    assert(!hdma_cpu_stalled(&h));

    puts("PASS: consecutive GDMA blocks");
}

static void test_progressive_hdma(void)
{
    setup();

    start(0x81);

    assert(hdma_is_active(&h));
    assert(!hdma_cpu_stalled(&h));

    hdma_tick(&h, 100);

    assert(m.vram[0][0] == 0);

    hdma_hblank(&h);

    assert(hdma_cpu_stalled(&h));

    hdma_tick(&h, 16);

    assert(m.vram[0][7] == 8);
    assert(m.vram[0][8] == 0);

    hdma_tick(&h, 16);

    assert(m.vram[0][15] == 16);

    assert(hdma_is_active(&h));
    assert(!hdma_cpu_stalled(&h));

    hdma_tick(&h, 100);

    assert(m.vram[0][16] == 0);

    hdma_hblank(&h);

    hdma_tick(&h, 32);

    assert(m.vram[0][31] == 32);

    assert(!hdma_is_active(&h));

    puts("PASS: progressive HBlank DMA");
}

static void test_vram_bank(void)
{
    setup();

    m.vbk = 1;

    start(0x00);

    hdma_tick(&h, 32);

    for (int i = 0; i < 16; i++)
    {
        assert(m.vram[1][i] == i + 1);
        assert(m.vram[0][i] == 0);
    }

    puts("PASS: progressive VRAM bank selection");
}

static void test_cancellation(void)
{
    setup();

    start(0x82);

    hdma_hblank(&h);

    hdma_tick(&h, 32);

    assert(hdma_is_active(&h));
    assert(!hdma_cpu_stalled(&h));

    mem_write(&m, 0xFF55, 0x00);

    assert(!hdma_is_active(&h));

    assert(mem_read(&m, 0xFF55) == 0x81);

    hdma_hblank(&h);

    hdma_tick(&h, 64);

    assert(m.vram[0][16] == 0);

    puts("PASS: progressive HDMA cancellation");
}

static void test_transfer_timing(void)
{
    setup();

    start(0x00);

    hdma_tick(&h, 31);

    assert(hdma_cpu_stalled(&h));
    assert(hdma_is_active(&h));

    assert(m.vram[0][14] == 15);
    assert(m.vram[0][15] == 0);

    hdma_tick(&h, 1);

    assert(m.vram[0][15] == 16);

    assert(!hdma_cpu_stalled(&h));
    assert(!hdma_is_active(&h));

    puts("PASS: 32-dot block timing");
}

int main(void)
{
    test_progressive_gdma();
    test_multiple_blocks();
    test_progressive_hdma();
    test_vram_bank();
    test_cancellation();
    test_transfer_timing();

    puts("\nAll progressive DMA tests passed.");

    return 0;
}
'''

if __name__ == "__main__":
    run_test("test_hdma_progressive", SOURCE)
