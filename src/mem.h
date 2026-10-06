#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"

typedef struct
{
    const Cart *cart;
    int cgb; /* 1 = Game Boy Color, 0 = original Game Boy (DMG) */

    uint8_t vram[2][0x2000]; /* 0x8000-0x9FFF; two banks on CGB, selected by VBK (0xFF4F) */
    uint8_t eram[0x2000];    /* 0xA000-0xBFFF (cartridge RAM) */
    uint8_t wram[8][0x1000]; /* 0xC000-0xCFFF = bank 0 (fixed)
                                0xD000-0xDFFF = bank 1-7, selected by SVBK (0xFF70) */
    uint8_t oam[0xA0];       /* 0xFE00-0xFE9F */
    uint8_t io[0x80];        /* 0xFF00-0xFF7F */
    uint8_t hram[0x7F];      /* 0xFF80-0xFFFE */
    uint8_t ie;              /* 0xFFFF */

    /* CGB-only registers */
    uint8_t vbk;          /* 0xFF4F: VRAM bank (bit 0) */
    uint8_t svbk;         /* 0xFF70: WRAM bank (bits 0-2) */
    uint8_t key1;         /* 0xFF4D: bit 0 = speed switch armed */
    uint8_t double_speed; /* 1 = CPU running at double speed (8 MHz) */

    /* timer */
    uint16_t div_counter; /* internal 16-bit counter; DIV (0xFF04) is its high byte */
    uint8_t timer_signal; /* last level of the signal that drives TIMA (to detect falling edges) */

    int ly_stub; /* 1 = LY always reads 0x90 (only for --doctor mode) */

    /* serial port output (Blargg's test ROMs report their results through it) */
    char serial_log[4096];
    int serial_len;
    int test_done; /* set to 1 once the log contains "Passed" or "Failed" */
} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif