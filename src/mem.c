#include "mem.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

void mem_init(Mem *mem, const Cart *cart)
{
    memset(mem, 0, sizeof(*mem));
    mem->cart = cart;
    /* Header byte 0x0143: 0x80 = CGB-compatible, 0xC0 = CGB-only */
    mem->cgb = (cart->data[0x0143] & 0x80) != 0;

    /* Video register values left behind by the boot ROM */
    mem->io[0x40] = 0x91; /* LCDC: LCD on, BG on, tile data at 0x8000 */
    mem->io[0x47] = 0xFC; /* BGP */
    mem->io[0x48] = 0xFF; /* OBP0 */
    mem->io[0x49] = 0xFF; /* OBP1 */
}

/* The DMG only has VRAM bank 0. */
static int vram_bank(const Mem *mem)
{
    return mem->cgb ? (mem->vbk & 1) : 0;
}

/* Bank mapped at 0xD000-0xDFFF. Writing 0 to SVBK selects bank 1.
   On the DMG, SVBK is never written, so the bank is always 1. */
static int wram_bank(const Mem *mem)
{
    int b = mem->svbk & 7;
    return b ? b : 1;
}

/* Echoes each byte sent over the serial port and logs it, so the end of
   Blargg's test ROMs can be detected. */
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
        /* ROM (no bank switching yet) */
        return addr < mem->cart->size ? mem->cart->data[addr] : 0xFF;
    }
    if (addr < 0xA000)
        return mem->vram[vram_bank(mem)][addr - 0x8000];
    if (addr < 0xC000)
        return mem->eram[addr - 0xA000];
    if (addr < 0xFE00)
    {
        /* WRAM and Echo RAM: 0xE000-0xFDFF mirrors 0xC000-0xDDFF.
           With the mask, 0xC000 and 0xE000 become offset 0; 0xD000 and 0xF000 become 0x1000. */
        uint16_t off = addr & 0x1FFF;
        return off < 0x1000 ? mem->wram[0][off]
                            : mem->wram[wram_bank(mem)][off - 0x1000];
    }
    if (addr < 0xFEA0)
        return mem->oam[addr - 0xFE00];
    if (addr < 0xFF00)
        return 0xFF; /* prohibited area */
    if (addr < 0xFF80)
    {
        switch (addr)
        {
        case 0xFF04:
            return (uint8_t)(mem->div_counter >> 8); /* DIV */
        case 0xFF07:
            return mem->io[0x07] | 0xF8; /* TAC: bits 3-7 always read 1 */
        case 0xFF0F:
            return mem->io[0x0F] | 0xE0; /* IF: bits 5-7 always read 1 */
        case 0xFF41:
            return mem->io[0x41] | 0x80; /* STAT: bit 7 always reads 1 */
        case 0xFF44:
            return mem->ly_stub ? 0x90 : mem->io[0x44]; /* LY (updated by the PPU) */
        case 0xFF4D:                                    /* KEY1: speed (CGB only) */
            return mem->cgb ? (uint8_t)(0x7E | mem->double_speed << 7 | (mem->key1 & 1)) : 0xFF;
        case 0xFF4F: /* VBK: VRAM bank (CGB only) */
            return mem->cgb ? (uint8_t)(0xFE | (mem->vbk & 1)) : 0xFF;
        case 0xFF70: /* SVBK: WRAM bank (CGB only) */
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
        return; /* ROM: writes ignored (they will drive the MBC later) */
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
        case 0xFF02: /* SC: starts a serial transfer */
            if (val == 0x81)
            {
                serial_putc(mem, mem->io[0x01]); /* SB: byte to send */
                mem->io[0x02] = 0x01;            /* transfer complete */
                return;
            }
            break;
        case 0xFF04: /* DIV: any write resets the counter */
            mem->div_counter = 0;
            timer_sync(mem); /* the signal dropping can increment TIMA */
            return;
        case 0xFF07: /* TAC */
            mem->io[0x07] = val & 7;
            timer_sync(mem); /* changing the frequency can also increment TIMA */
            return;
        case 0xFF41: /* STAT: only bits 3-6 are writable */
            mem->io[0x41] = (uint8_t)((mem->io[0x41] & 0x07) | (val & 0x78));
            return;
        case 0xFF44:
            return; /* LY: read-only */
        case 0xFF46:
        { /* DMA: copies 160 bytes into OAM */
            uint16_t src = (uint16_t)(val << 8);
            for (int i = 0; i < 0xA0; i++)
            {
                mem->oam[i] = mem_read(mem, (uint16_t)(src + i));
            }
            mem->io[0x46] = val;
            return;
        }
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