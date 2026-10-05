#include "mem.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

void mem_init(Mem *mem, const Cart *cart)
{
    memset(mem, 0, sizeof(*mem));
    mem->cart = cart;
    mem->cgb = (cart->data[0x0143] & 0x80) != 0;
}

static int vram_bank(const Mem *mem)
{
    return mem->cgb ? (mem->vbk & 1) : 0;
}

static int wram_bank(const Mem *mem)
{
    int b = mem->svbk & 7;
    return b ? b : 1;
}

static void serial_putc(Mem *mem, uint8_t c)
{
    fputc(c, stderr);
    fflush(stderr);

    if (mem->serial_len < (int)sizeof(mem->serial_log) - 1)
    {
        mem->serial_log[mem->serial_len++] = (char)c;
        mem->serial_log[mem->serial_len] = '\0';
    }
    if (strstr(mem->serial_log, "Passed") || strstr(mem->serial_log, "Failed"))
    {
        mem->test_done = 1;
    }
}

uint8_t mem_read(const Mem *mem, uint16_t addr)
{
    if (addr < 0x8000)
    {
        /* ROM (without Bank Switching) */
        return addr < mem->cart->size ? mem->cart->data[addr] : 0xFF;
    }
    if (addr < 0xA000)
        return mem->vram[vram_bank(mem)][addr - 0x8000];
    if (addr < 0xC000)
        return mem->eram[addr - 0xA000];
    if (addr < 0xFE00)
    {
        /* WRAM and Echo RAM: 0xE000-0xFDFF mirrors 0xC000-0xDDFF.
           0xC000 e 0xE000 turns offset 0; 0xD000 and 0xF000 turns 0x1000. */
        uint16_t off = addr & 0x1FFF;
        return off < 0x1000 ? mem->wram[0][off]
                            : mem->wram[wram_bank(mem)][off - 0x1000];
    }
    if (addr < 0xFEA0)
        return mem->oam[addr - 0xFE00];
    if (addr < 0xFF00)
        return 0xFF;
    if (addr < 0xFF80)
    {
        switch (addr)
        {
        case 0xFF04:
            return (uint8_t)(mem->div_counter >> 8); /* DIV */
        case 0xFF07:
            return mem->io[0x07] | 0xF8; /* TAC: bits 3-7 */
        case 0xFF0F:
            return mem->io[0x0F] | 0xE0; /* IF: bits 5-7 */
        case 0xFF44:
            return 0x90; /* LY: provisory until PPU doesn't exists */
        case 0xFF4D:
            return mem->cgb ? (uint8_t)(0x7E | mem->double_speed << 7 | (mem->key1 & 1)) : 0xFF;
        case 0xFF4F:
            return mem->cgb ? (uint8_t)(0xFE | (mem->vbk & 1)) : 0xFF;
        case 0xFF70:
            return mem->cgb ? (uint8_t)(0xF8 | (mem->svbk & 7)) : 0xFF;
        default:
            return mem->io[addr - 0xFF00];
        }
    }
    if (addr < 0xFFFF)
        return mem->hram[addr - 0xFF80];
    return mem->ie;
}

void mem_write(Mem *mem, uint16_t addr, uint8_t val)
{
    if (addr < 0x8000)
        return;
    if (addr < 0xA000)
    {
        mem->vram[vram_bank(mem)][addr - 0x8000] = val;
        return;
    }
    if (addr < 0xC000)
    {
        mem->eram[addr - 0xA000] = val;
        return;
    }
    if (addr < 0xFE00)
    {
        uint16_t off = addr & 0x1FFF;
        if (off < 0x1000)
            mem->wram[0][off] = val;
        else
            mem->wram[wram_bank(mem)][off - 0x1000] = val;
        return;
    }
    if (addr < 0xFEA0)
    {
        mem->oam[addr - 0xFE00] = val;
        return;
    }
    if (addr < 0xFF00)
        return;
    if (addr < 0xFF80)
    {
        switch (addr)
        {
        case 0xFF02:
            if (val == 0x81)
            {
                serial_putc(mem, mem->io[0x01]);
                mem->io[0x02] = 0x01;
                return;
            }
            break;
        case 0xFF04:
            mem->div_counter = 0;
            timer_sync(mem);
            return;
        case 0xFF07: /* TAC */
            mem->io[0x07] = val & 7;
            timer_sync(mem);
            return;
        case 0xFF4D:
            if (mem->cgb)
                mem->key1 = val & 1;
            return;
        case 0xFF4F:
            if (mem->cgb)
                mem->vbk = val & 1;
            return;
        case 0xFF70:
            if (mem->cgb)
                mem->svbk = val & 7;
            return;
        default:
            break;
        }
        mem->io[addr - 0xFF00] = val;
        return;
    }
    if (addr < 0xFFFF)
    {
        mem->hram[addr - 0xFF80] = val;
        return;
    }
    mem->ie = val;
}
