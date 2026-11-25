
#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "../mem.h"
#include "../ppu/fifo.h"
#include "../ppu/fetcher.h"

#define LCD_W 160
#define LCD_H 144

typedef struct
{
    Mem *mem;
    uint32_t fb[LCD_W * LCD_H];

    int lcd_on;
    int dot;
    int mode;

    int window_started;
    int window_line;
    int wy_triggered;
    int stat_line;
    int off_dots;
    int frame_ready;

    uint64_t frames;

    /* Background pixel pipeline */
    PpuFifo bg_fifo;
    PpuFetcher fetcher;

    int output_x;
    int fetch_discard;

    /* Background information used by OBJ priority */
    uint8_t bg_id[LCD_W];
    uint8_t bg_prio[LCD_W];

} Ppu;

void ppu_init(Ppu *ppu, Mem *mem);

void ppu_tick(Ppu *ppu, int dots);

#endif
