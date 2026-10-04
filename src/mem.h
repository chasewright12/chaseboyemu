#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"

typedef struct {
    const Cart *cart;
    uint8_t vram[0x2000];  /* 0x8000-0x9FFF */
    uint8_t eram[0x2000];  /* 0xA000-0xBFFF (RAM do cartucho) */
    uint8_t wram[0x2000];  /* 0xC000-0xDFFF */
    uint8_t oam[0xA0];     /* 0xFE00-0xFE9F */
    uint8_t io[0x80];      /* 0xFF00-0xFF7F */
    uint8_t hram[0x7F];    /* 0xFF80-0xFFFE */
    uint8_t ie;            /* 0xFFFF */
} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif
