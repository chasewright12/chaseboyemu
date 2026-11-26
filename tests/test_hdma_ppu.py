from test_hdma import run_test

SOURCE = r'''
#include <assert.h>
#include <stdio.h>

/*
 * Import the actual emulator scheduler without
 * conflicting with the test's main function.
 */
#define main emulator_main
#include "main.c"
#undef main

static Emu e;

static void setup(void)
{
    memset(&e, 0, sizeof(e));

    e.mem.cgb = 1;

    e.mem.io[0x40] = 0x91;

    hdma_init(&e.hdma, &e.mem);

    e.mem.hdma = &e.hdma;

    e.cpu.mem = &e.mem;

    e.cpu.pc = 0xC100;

    ppu_init(&e.ppu, &e.mem);

    for (int i = 0; i < 32; ++i)
    {
        mem_write(
            &e.mem,
            0xC000 + i,
            (uint8_t)(i + 1));
    }
}

static void start(uint8_t control)
{
    mem_write(&e.mem, 0xFF51, 0xC0);
    mem_write(&e.mem, 0xFF52, 0);
    mem_write(&e.mem, 0xFF53, 0);
    mem_write(&e.mem, 0xFF54, 0);
    mem_write(&e.mem, 0xFF55, control);
}

static void test_stall(int speed)
{
    setup();

    e.mem.double_speed = speed;

    start(0);

    uint16_t pc = e.cpu.pc;

    uint64_t before = e.cpu.cycles;

    int elapsed = 0;

    /*
     * CPU must not execute instructions
     * while DMA is stalling it.
     */
    while (hdma_cpu_stalled(&e.hdma))
    {
        int cycles = emu_step(&e);

        assert(cycles == 4);

        elapsed += cycles;

        assert(e.cpu.pc == pc);

        assert(elapsed <= 64);
    }

    /*
     * Normal speed: 32 CPU T-cycles.
     * Double speed: 64 CPU T-cycles.
     */
    assert(elapsed == (speed ? 64 : 32));

    assert(
        e.cpu.cycles - before ==
        (uint64_t)elapsed);

    /*
     * PPU must advance by 32 dots
     * regardless of CPU speed.
     */
    assert(e.ppu.dot == 32);

    /*
     * CPU resumes execution.
     * Zeroed WRAM contains a NOP instruction.
     */
    assert(emu_step(&e) == 4);

    assert(e.cpu.pc == pc + 1);

    puts(
        speed
            ? "PASS: double-speed stall"
            : "PASS: normal-speed stall");
}

int main(void)
{
    /*
     * TEST 1:
     * Automatic HBlank DMA through the PPU.
     */
    setup();

    start(0x81);

    ppu_tick(&e.ppu, 80);

    assert(e.ppu.mode == 3);

    assert(e.mem.vram[0][0] == 0);

    for (
        int i = 0;
        i < 376 && e.ppu.mode == 3;
        ++i)
    {
        ppu_tick(&e.ppu, 1);
    }

    assert(e.ppu.mode == 0);

    assert(e.mem.vram[0][15] == 16);

    assert(e.mem.vram[0][16] == 0);

    hdma_tick(&e.hdma, 32);

    ppu_tick(&e.ppu, 456 - e.ppu.dot);

    assert(e.mem.vram[0][16] == 0);

    ppu_tick(&e.ppu, 456);

    assert(e.mem.vram[0][31] == 32);

    puts("PASS: one block per visible HBlank");

    /*
     * TEST 2:
     * No HBlank DMA during VBlank.
     */
    setup();

    ppu_tick(&e.ppu, 144 * 456);

    assert(e.ppu.mode == 1);

    start(0x80);

    ppu_tick(&e.ppu, 9 * 456);

    assert(e.mem.vram[0][0] == 0);

    assert(hdma_is_active(&e.hdma));

    puts("PASS: no HBlank transfer during VBlank");

    /*
     * TEST 3:
     * CPU stall in normal speed mode.
     */
    test_stall(0);

    /*
     * TEST 4:
     * CPU stall in double speed mode.
     */
    test_stall(1);

    return 0;
}
'''

if __name__ == '__main__':
    run_test('test_hdma_ppu', SOURCE)
