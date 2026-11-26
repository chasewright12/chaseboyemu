
#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "cart.h"
#include "cartridge/cartridge.h"

typedef struct Hdma Hdma;

typedef struct
{
    Cartridge cartridge;
    const Cart *cart;
    int cgb;

    uint8_t vram[2][0x2000];
    uint8_t wram[8][0x1000];
    uint8_t oam[0xA0];
    uint8_t io[0x80];
    uint8_t hram[0x7F];
    uint8_t ie;

    uint8_t vbk;
    uint8_t svbk;
    uint8_t key1;
    uint8_t double_speed;
    uint8_t bg_pal[64];
    uint8_t obj_pal[64];
    uint8_t bcps;
    uint8_t ocps;
    uint8_t opri;

    uint8_t ppu_mode;

    uint16_t div_counter;
    uint8_t timer_signal;

    uint8_t joy_select;
    uint8_t joy_state;

    int ly_stub;

    char serial_log[4096];
    int serial_len;
    int test_done;

    Hdma *hdma;

} Mem;

void mem_init(Mem *mem, const Cart *cart);
uint8_t mem_read(const Mem *mem, uint16_t addr);
void mem_write(Mem *mem, uint16_t addr, uint8_t val);

#endif
