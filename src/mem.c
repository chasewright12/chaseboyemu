#include "mem.h"
#include <string.h>

void mem_init(Mem *mem, const Cart *cart) {
    memset(mem, 0, sizeof(*mem));
    mem->cart = cart;
}

uint8_t mem_read(const Mem *mem, uint16_t addr) {
    if (addr < 0x8000) {
        /* ROM (por enquanto sem bank switching) */
        return addr < mem->cart->size ? mem->cart->data[addr] : 0xFF;
    }
    if (addr < 0xA000) return mem->vram[addr - 0x8000];
    if (addr < 0xC000) return mem->eram[addr - 0xA000];
    if (addr < 0xE000) return mem->wram[addr - 0xC000];
    if (addr < 0xFE00) return mem->wram[addr - 0xE000];  /* echo da WRAM */
    if (addr < 0xFEA0) return mem->oam[addr - 0xFE00];
    if (addr < 0xFF00) return 0xFF;                      /* area proibida */
    if (addr < 0xFF80) return mem->io[addr - 0xFF00];
    if (addr < 0xFFFF) return mem->hram[addr - 0xFF80];
    return mem->ie;
}

void mem_write(Mem *mem, uint16_t addr, uint8_t val) {
    if (addr < 0x8000) return;  /* ROM: escrita ignorada (depois vira controle do MBC) */
    if (addr < 0xA000) { mem->vram[addr - 0x8000] = val; return; }
    if (addr < 0xC000) { mem->eram[addr - 0xA000] = val; return; }
    if (addr < 0xE000) { mem->wram[addr - 0xC000] = val; return; }
    if (addr < 0xFE00) { mem->wram[addr - 0xE000] = val; return; }
    if (addr < 0xFEA0) { mem->oam[addr - 0xFE00] = val; return; }
    if (addr < 0xFF00) return;
    if (addr < 0xFF80) { mem->io[addr - 0xFF00] = val; return; }
    if (addr < 0xFFFF) { mem->hram[addr - 0xFF80] = val; return; }
    mem->ie = val;
}
