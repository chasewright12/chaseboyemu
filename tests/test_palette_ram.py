
import subprocess
import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"

static Mem mem;

static void test_bg_palette(void)
{
    mem.bcps = 0x00;
    mem_write(&mem, 0xFF69, 0x34);

    assert(mem.bg_pal[0] == 0x34);
    assert(mem_read(&mem, 0xFF69) == 0x34);

    printf("[PASS] BG palette read/write\n");
}

static void test_obj_palette(void)
{
    mem.ocps = 0x00;
    mem_write(&mem, 0xFF6B, 0x56);

    assert(mem.obj_pal[0] == 0x56);
    assert(mem_read(&mem, 0xFF6B) == 0x56);

    printf("[PASS] OBJ palette read/write\n");
}

static void test_bg_auto_increment(void)
{
    mem.bcps = 0x80;
    mem_write(&mem, 0xFF69, 0x12);

    assert(mem.bg_pal[0] == 0x12);
    assert((mem.bcps & 0x3F) == 1);

    printf("[PASS] BG palette auto-increment\n");
}

static void test_obj_auto_increment(void)
{
    mem.ocps = 0x80;
    mem_write(&mem, 0xFF6B, 0x23);

    assert(mem.obj_pal[0] == 0x23);
    assert((mem.ocps & 0x3F) == 1);

    printf("[PASS] OBJ palette auto-increment\n");
}

static void test_bg_wraparound(void)
{
    mem.bcps = 0xBF;
    mem_write(&mem, 0xFF69, 0xAA);

    assert(mem.bg_pal[63] == 0xAA);
    assert((mem.bcps & 0x3F) == 0);
    assert(mem.bcps & 0x80);

    printf("[PASS] BG palette index wraparound\n");
}

static void test_obj_wraparound(void)
{
    mem.ocps = 0xBF;
    mem_write(&mem, 0xFF6B, 0xBB);

    assert(mem.obj_pal[63] == 0xBB);
    assert((mem.ocps & 0x3F) == 0);
    assert(mem.ocps & 0x80);

    printf("[PASS] OBJ palette index wraparound\n");
}

static void test_mode3_bg_block(void)
{
    mem.bcps = 0x00;
    mem.bg_pal[0] = 0x45;
    mem.ppu_mode = 3;

    assert(mem_read(&mem, 0xFF69) == 0xFF);

    mem_write(&mem, 0xFF69, 0x99);

    assert(mem.bg_pal[0] == 0x45);

    printf("[PASS] BG palette blocked during Mode 3\n");
}

static void test_mode3_obj_block(void)
{
    mem.ocps = 0x00;
    mem.obj_pal[0] = 0x67;
    mem.ppu_mode = 3;

    assert(mem_read(&mem, 0xFF6B) == 0xFF);

    mem_write(&mem, 0xFF6B, 0x99);

    assert(mem.obj_pal[0] == 0x67);

    printf("[PASS] OBJ palette blocked during Mode 3\n");
}

static void test_auto_increment_blocked(void)
{
    mem.bcps = 0x80;
    mem.ocps = 0x80;
    mem.ppu_mode = 3;

    mem_write(&mem, 0xFF69, 0x11);
    mem_write(&mem, 0xFF6B, 0x22);

    assert((mem.bcps & 0x3F) == 0);
    assert((mem.ocps & 0x3F) == 0);

    printf("[PASS] Auto-increment blocked during Mode 3\n");
}

static void test_other_modes(void)
{
    const int modes[] = {0, 1, 2};

    for (unsigned int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        mem.ppu_mode = (uint8_t)modes[i];

        mem.bcps = 0;
        mem_write(&mem, 0xFF69, 0x30 + modes[i]);

        assert(mem.bg_pal[0] == 0x30 + modes[i]);
        assert(mem_read(&mem, 0xFF69) == 0x30 + modes[i]);
    }

    printf("[PASS] Palette access during Modes 0, 1 and 2\n");
}

static void test_palette_registers(void)
{
    mem_write(&mem, 0xFF68, 0xBF);
    assert(mem_read(&mem, 0xFF68) == 0xFF);

    mem_write(&mem, 0xFF6A, 0xBF);
    assert(mem_read(&mem, 0xFF6A) == 0xFF);

    printf("[PASS] BGPI and OBPI registers\n");
}

int main(void)
{
    memset(&mem, 0, sizeof(mem));
    mem.cgb = 1;
    mem.ppu_mode = 0;

    test_bg_palette();
    test_obj_palette();
    test_bg_auto_increment();
    test_obj_auto_increment();
    test_bg_wraparound();
    test_obj_wraparound();
    test_mode3_bg_block();
    test_mode3_obj_block();
    test_auto_increment_blocked();
    test_other_modes();
    test_palette_registers();

    printf("\nAll palette RAM tests passed!\n");

    return 0;
}
"""


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        source = tmp / "test_palette_ram.c"
        executable = tmp / "test_palette_ram"

        source.write_text(HARNESS)

        compile_cmd = [
            "gcc",
            "-Wall",
            "-Wextra",
            "-std=c11",
            "-g",
            "-O0",
            "-I", str(SRC),
            str(source),
            str(SRC / "mem.c"),
            str(SRC / "cart.c"),
            str(SRC / "timer.c"),
            str(SRC / "joypad.c"),
            str(SRC / "cartridge" / "cartridge.c"),
            str(SRC / "cartridge" / "mbc1.c"),
            str(SRC / "cartridge" / "mbc3.c"),
            str(SRC / "cartridge" / "mbc5.c"),
            "-o", str(executable),
        ]

        print("Compiling palette RAM tests...\n")

        result = subprocess.run(
            compile_cmd,
            cwd=ROOT,
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            print("Compilation failed:")
            print(result.stderr)
            sys.exit(1)

        print("Compilation successful.\n")
        print("Running tests...\n")

        result = subprocess.run(
            [str(executable)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )

        print(result.stdout, end="")

        if result.returncode != 0:
            print("\nTest execution failed:")
            print(result.stderr)
            sys.exit(result.returncode)

        print("\nSUCCESS: palette RAM tests completed.")


if __name__ == "__main__":
    main()
