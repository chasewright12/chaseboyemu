import ctypes
import subprocess
import tempfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
PPU_DIR = ROOT / "src" / "ppu"

BRIDGE_SOURCE = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ppu/fetcher.h"
#include "ppu/ppu.h"

typedef struct {
    Mem mem;
    PpuFetcher fetcher;
    PpuFifo fifo;
    Ppu ppu;
} TestRig;

TestRig *rig_create(void) {
    TestRig *r = (TestRig *)calloc(1, sizeof(TestRig));
    if (!r) return NULL;
    ppu_fifo_init(&r->fifo);
    return r;
}

void rig_destroy(TestRig *r) { free(r); }

void rig_set_cgb(TestRig *r, int value) {
    if (r) r->mem.cgb = value != 0;
}

void rig_set_vram(TestRig *r, int bank, int offset, uint8_t value) {
    if (!r || bank < 0 || bank > 1 || offset < 0 || offset >= 0x2000) return;
    r->mem.vram[bank][offset] = value;
}

uint8_t rig_get_vram(TestRig *r, int bank, int offset) {
    if (!r || bank < 0 || bank > 1 || offset < 0 || offset >= 0x2000) return 0;
    return r->mem.vram[bank][offset];
}

void rig_set_io(TestRig *r, int offset, uint8_t value) {
    if (r && offset >= 0 && offset < 0x80) r->mem.io[offset] = value;
}

uint8_t rig_get_io(TestRig *r, int offset) {
    if (!r || offset < 0 || offset >= 0x80) return 0;
    return r->mem.io[offset];
}

void rig_set_bg_palette(TestRig *r, int offset, uint8_t value) {
    if (r && offset >= 0 && offset < 64) r->mem.bg_pal[offset] = value;
}

void rig_fetcher_init(TestRig *r, uint8_t x, uint8_t y, int window) {
    if (!r) return;
    ppu_fifo_clear(&r->fifo);
    ppu_fetcher_init(&r->fetcher, x, y, window);
}

void rig_fetcher_reset(TestRig *r) {
    if (r) ppu_fetcher_reset(&r->fetcher);
}

void rig_fetcher_tick(TestRig *r) {
    if (r) ppu_fetcher_tick(&r->fetcher, &r->mem, &r->fifo);
}

int rig_fetcher_state(TestRig *r) { return r ? (int)r->fetcher.state : -1; }
int rig_fetcher_phase(TestRig *r) { return r ? r->fetcher.phase : -1; }
int rig_fetcher_tile_x(TestRig *r) { return r ? r->fetcher.tile_x : -1; }
int rig_fetcher_tile_number(TestRig *r) { return r ? r->fetcher.tile_number : -1; }
int rig_fetcher_tile_attr(TestRig *r) { return r ? r->fetcher.tile_attr : -1; }
int rig_fetcher_tile_bank(TestRig *r) { return r ? r->fetcher.tile_bank : -1; }
int rig_fetcher_tile_row(TestRig *r) { return r ? r->fetcher.tile_row : -1; }
int rig_fetcher_data_low(TestRig *r) { return r ? r->fetcher.data_low : -1; }
int rig_fetcher_data_high(TestRig *r) { return r ? r->fetcher.data_high : -1; }
int rig_fetcher_tile_data_addr(TestRig *r) { return r ? r->fetcher.tile_data_addr : -1; }
int rig_fetcher_tile_map_addr(TestRig *r) { return r ? r->fetcher.tile_map_addr : -1; }

unsigned int rig_fifo_size(TestRig *r) {
    return r ? ppu_fifo_size(&r->fifo) : 0;
}

int rig_fifo_push_color(TestRig *r, uint8_t color) {
    if (!r) return 0;
    PpuPixel p = {0};
    p.color_id = color;
    return ppu_fifo_push(&r->fifo, p);
}

int rig_fifo_pop(TestRig *r, int *color, int *palette, int *priority, int *bank) {
    if (!r) return 0;
    PpuPixel p;
    if (!ppu_fifo_pop(&r->fifo, &p)) return 0;
    if (color) *color = p.color_id;
    if (palette) *palette = p.palette;
    if (priority) *priority = p.priority;
    if (bank) *bank = p.bank;
    return 1;
}

void rig_ppu_init(TestRig *r) {
    if (r) ppu_init(&r->ppu, &r->mem);
}

void rig_ppu_tick(TestRig *r, int dots) {
    if (r) ppu_tick(&r->ppu, dots);
}

uint32_t rig_framebuffer_pixel(TestRig *r, int x, int y) {
    if (!r || x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return 0;
    return r->ppu.fb[y * LCD_W + x];
}

int rig_ppu_mode(TestRig *r) { return r ? r->ppu.mode : -1; }
int rig_ppu_dot(TestRig *r) { return r ? r->ppu.dot : -1; }
int rig_ppu_ly(TestRig *r) { return r ? r->mem.io[0x44] : -1; }
int rig_ppu_frame_ready(TestRig *r) { return r ? r->ppu.frame_ready : 0; }
uint64_t rig_ppu_frames(TestRig *r) { return r ? r->ppu.frames : 0; }
"""

@pytest.fixture(scope="session")
def ppu_lib():
    sources = [
        PPU_DIR / "fifo.c",
        PPU_DIR / "fetcher.c",
        PPU_DIR / "ppu.c",
    ]
    for source in sources:
        if not source.exists():
            pytest.fail(f"PPU source not found: {source}")

    with tempfile.TemporaryDirectory(prefix="gbemu_ppu_") as temp:
        temp_path = Path(temp)
        bridge = temp_path / "ppu_test_bridge.c"
        library = temp_path / "libgbemu_ppu_test.so"
        bridge.write_text(BRIDGE_SOURCE)

        command = [
            "gcc", "-shared", "-fPIC", "-Wall", "-Wextra",
            "-std=c11", "-g", "-O2",
            "-I", str(ROOT / "src"),
            "-I", str(PPU_DIR),
            str(bridge),
            *(str(s) for s in sources),
            "-o", str(library),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            pytest.fail("PPU test bridge compilation failed:\n" + result.stderr)

        lib = ctypes.CDLL(str(library))
        lib.rig_create.restype = ctypes.c_void_p
        lib.rig_destroy.argtypes = [ctypes.c_void_p]

        for name in ("rig_set_cgb",):
            getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_int]

        lib.rig_set_vram.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_uint8]
        lib.rig_get_vram.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.rig_get_vram.restype = ctypes.c_uint8
        lib.rig_set_io.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_uint8]
        lib.rig_get_io.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.rig_get_io.restype = ctypes.c_uint8
        lib.rig_set_bg_palette.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_uint8]

        lib.rig_fetcher_init.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_int]
        lib.rig_fetcher_reset.argtypes = [ctypes.c_void_p]
        lib.rig_fetcher_tick.argtypes = [ctypes.c_void_p]

        for name in (
            "rig_fetcher_state", "rig_fetcher_phase", "rig_fetcher_tile_x",
            "rig_fetcher_tile_number", "rig_fetcher_tile_attr",
            "rig_fetcher_tile_bank", "rig_fetcher_tile_row",
            "rig_fetcher_data_low", "rig_fetcher_data_high",
            "rig_fetcher_tile_data_addr", "rig_fetcher_tile_map_addr",
            "rig_ppu_mode", "rig_ppu_dot", "rig_ppu_ly",
            "rig_ppu_frame_ready",
        ):
            getattr(lib, name).argtypes = [ctypes.c_void_p]
            getattr(lib, name).restype = ctypes.c_int

        lib.rig_fifo_size.argtypes = [ctypes.c_void_p]
        lib.rig_fifo_size.restype = ctypes.c_uint
        lib.rig_fifo_push_color.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
        lib.rig_fifo_push_color.restype = ctypes.c_int
        lib.rig_fifo_pop.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
        ]
        lib.rig_fifo_pop.restype = ctypes.c_int

        lib.rig_ppu_init.argtypes = [ctypes.c_void_p]
        lib.rig_ppu_tick.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.rig_framebuffer_pixel.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.rig_framebuffer_pixel.restype = ctypes.c_uint32
        lib.rig_ppu_frames.argtypes = [ctypes.c_void_p]
        lib.rig_ppu_frames.restype = ctypes.c_uint64

        yield lib


@pytest.fixture
def rig(ppu_lib):
    handle = ppu_lib.rig_create()
    assert handle
    try:
        yield handle
    finally:
        ppu_lib.rig_destroy(handle)


def fetcher_advance(lib, rig, dots):
    for _ in range(dots):
        lib.rig_fetcher_tick(rig)


def fifo_pop(lib, rig):
    color = ctypes.c_int()
    palette = ctypes.c_int()
    priority = ctypes.c_int()
    bank = ctypes.c_int()
    ok = lib.rig_fifo_pop(
        rig, ctypes.byref(color), ctypes.byref(palette),
        ctypes.byref(priority), ctypes.byref(bank)
    )
    if not ok:
        return None
    return color.value, palette.value, priority.value, bank.value