from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run_test(name, source):
    src = ROOT / 'src'

    files = sorted(
        p for p in src.rglob('*.c')
        if p.name != 'main.c'
    )

    with tempfile.TemporaryDirectory() as tmp:
        cfile = Path(tmp) / (name + '.c')
        exe = Path(tmp) / name

        cfile.write_text(source, encoding='utf-8')

        cmd = [
            'gcc',
            '-std=c11',
            '-Wall',
            '-Wextra',
            '-g',
            '-I', str(src),
            '-I', str(src / 'ppu'),
            '-I', str(src / 'dma'),
            str(cfile),
            *map(str, files),
            '-o', str(exe)
        ]

        subprocess.run(cmd, cwd=ROOT, check=True)

        subprocess.run(
            [str(exe)],
            cwd=ROOT,
            check=True,
            timeout=20
        )


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

    for (int i = 0; i < 64; ++i)
    {
        mem_write(
            &m,
            0xC000 + i,
            (uint8_t)(i + 1));
    }

    assert(mem_read(&m, 0xFF55) == 0xFF);
}

static void start(uint8_t control)
{
    mem_write(&m, 0xFF51, 0xC0);
    mem_write(&m, 0xFF52, 0x00);
    mem_write(&m, 0xFF53, 0x00);
    mem_write(&m, 0xFF54, 0x00);
    mem_write(&m, 0xFF55, control);
}

int main(void)
{
    /*
     * TEST 1:
     * General Purpose DMA, VRAM bank and CPU stall.
     */
    setup();

    m.vbk = 1;

    start(0x01);

    for (int i = 0; i < 32; ++i)
    {
        assert(m.vram[1][i] == i + 1);
        assert(m.vram[0][i] == 0);
    }

    assert(mem_read(&m, 0xFF55) == 0xFF);

    assert(hdma_cpu_stalled(&h));
    assert(h.stall_dots == 64);

    hdma_tick(&h, 63);

    assert(hdma_cpu_stalled(&h));

    hdma_tick(&h, 1);

    assert(!hdma_cpu_stalled(&h));

    puts("PASS: GDMA, VRAM bank and stall");

    /*
     * TEST 2:
     * HBlank DMA block transfer.
     */
    setup();

    start(0x81);

    assert(hdma_is_active(&h));

    assert(mem_read(&m, 0xFF55) == 1);

    assert(m.vram[0][0] == 0);

    hdma_hblank(&h);

    for (int i = 0; i < 16; ++i)
    {
        assert(m.vram[0][i] == i + 1);
    }

    assert(m.vram[0][16] == 0);

    assert(mem_read(&m, 0xFF55) == 0);

    assert(h.stall_dots == 32);

    hdma_tick(&h, 32);

    hdma_hblank(&h);

    assert(m.vram[0][31] == 32);

    assert(!hdma_is_active(&h));

    assert(mem_read(&m, 0xFF55) == 0xFF);

    puts("PASS: HBlank blocks and completion");

    /*
     * TEST 3:
     * HBlank DMA cancellation.
     */
    setup();

    start(0x82);

    hdma_hblank(&h);

    hdma_tick(&h, 32);

    mem_write(&m, 0xFF55, 0);

    assert(!hdma_is_active(&h));

    assert(mem_read(&m, 0xFF55) == 0x81);

    hdma_hblank(&h);

    assert(m.vram[0][16] == 0);

    puts("PASS: cancellation between blocks");

    return 0;
}
'''

if __name__ == '__main__':
    run_test('test_hdma', SOURCE)
