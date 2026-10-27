#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"

typedef struct
{
    const Cart *cart;
    int cgb;

    uint8_t vram[2][0x2000]; /* 0x8000-0x9FFF; two banks on CGB, selected by VBK (0xFF4F) */
    uint8_t eram[0x2000];
    uint8_t wram[8][0x1000]; /* 0xC000-0xCFFF = bank 0 (fixed)
                                0xD000-0xDFFF = bank 1-7, selected by SVBK (0xFF70) */
    uint8_t oam[0xA0];
    uint8_t io[0x80];
    uint8_t hram[0x7F];
    uint8_t ie;

    uint8_t vbk;          /* 0xFF4F: VRAM bank (bit 0) */
    uint8_t svbk;         /* 0xFF70: WRAM bank (bits 0-2) */
    uint8_t key1;         /* 0xFF4D: bit 0 = speed switch armed */
    uint8_t double_speed; /* 1 = CPU running at double speed (8 MHz) */
    uint8_t bg_pal[64];   /* background palette RAM: 8 palettes x 4 colors x 2 bytes (RGB555) */
    uint8_t obj_pal[64];
    uint8_t bcps; /* 0xFF68: background palette index (bit 7 = auto-increment) */
    uint8_t ocps;
    uint8_t opri; /* 0xFF6C: bit 0 = 1 -> sprite priority by X (DMG style) */

    uint16_t div_counter; /* internal 16-bit counter; DIV (0xFF04) is its high byte */
    uint8_t timer_signal; /* last level of the signal that drives TIMA (to detect falling edges) */

    uint8_t joy_select; /* bits 4-5 of 0xFF00: which button group is selected (0 = selected) */
    uint8_t joy_state;  /* one bit per button, 1 = pressed (bit positions in joypad.h) */

    int ly_stub; /* 1 = LY always reads 0x90 (only for --doctor mode) */

    char serial_log[4096];
    int serial_len;
    int test_done; /* set to 1 once the log contains "Passed" or "Failed" */
} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif