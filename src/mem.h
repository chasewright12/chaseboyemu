#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"

typedef struct
{
    const Cart *cart;
    int cgb;

    uint8_t vram[2][0x2000];
    uint8_t eram[0x2000];
    uint8_t wram[8][0x1000];
    uint8_t oam[0xA0];
    uint8_t io[0x80];
    uint8_t hram[0x7F];
    uint8_t ie;
    uint8_t vbk;
    uint8_t svbk;
    uint8_t key1;
    uint8_t double_speed;
    char serial_log[4096];
    int serial_len;
    int test_done;
} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif