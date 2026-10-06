#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "mem.h"

#define LCD_W 160
#define LCD_H 144

typedef struct
{
    Mem *mem;
    uint32_t fb[LCD_W * LCD_H]; /* finished frame, pixels as 0xAARRGGBB */

    int lcd_on;       /* mirrors LCDC bit 7 */
    int dot;          /* position within the current line: 0-455 */
    int mode;         /* 0 HBlank, 1 VBlank, 2 OAM scan, 3 drawing */
    int window_line;  /* internal window line counter */
    int wy_triggered; /* set once LY has reached WY during the current frame */
    int stat_line;    /* last level of the STAT interrupt line (to detect rising edges) */
    int off_dots;     /* while the LCD is off, counts the time of one frame */
    int frame_ready;  /* set to 1 when a new frame is available in fb */
    uint64_t frames;  /* frames produced so far */
} Ppu;

void ppu_init(Ppu *ppu, Mem *mem);

/* Advances the PPU by the given number of dots (PPU clock cycles).
   At normal speed 1 CPU cycle = 1 dot; in CGB double speed, 2 CPU cycles = 1 dot. */
void ppu_tick(Ppu *ppu, int dots);

#endif