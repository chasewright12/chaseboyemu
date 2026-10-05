#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"

typedef struct {
    const Cart *cart;
    int cgb;                   /* 1 = Game Boy Color, 0 = Game Boy original (DMG) */

    uint8_t vram[2][0x2000];   /* 0x8000-0x9FFF       */
    uint8_t eram[0x2000];     /* 0xA000-0xBFFF */
    uint8_t wram[8][0x1000];    /* 0xC000-0xCFFF
    0xD000-0xDFFF
    0xFF70) */
    uint8_t oam[0xA0];         /* 0xFE00-0xFE9F */
    uint8_t io[0x80];          /* 0xFF00-0xFF7F */
    uint8_t hram[0x7F];        /* 0xFF80-0xFFFE */
    uint8_t ie;                /* 0xFFFF */
    uint8_t vbk;               /* 0xFF4F */
    uint8_t svbk;              /* 0xFF70 */
    uint8_t key1;              /* 0xFF4D */
    uint8_t double_speed;      /* 1 = CPU dual-channel (8 MHz) */

    /* timer */
    uint16_t div_counter;      /* internal counter of 16 bits; DIV (0xFF04) and high byte */
    uint8_t timer_signal;      /* las value from signal */
    char serial_log[4096];
    int serial_len;
    int test_done;
} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif
