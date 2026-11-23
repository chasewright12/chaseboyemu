#include "mem.h"
#include "timer.h"
#include "joypad.h"
#include "cartridge/cartridge.h"

#include <stdio.h>
#include <string.h>

static int vram_bank(const Mem *mem)
{
    return mem->cgb ? (mem->vbk & 1) : 0;
}

static int wram_bank(const Mem *mem)
{
    int bank = mem->svbk & 7;
    return bank ? bank : 1;
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

    if (strstr(mem->serial_log, "Passed") ||
        strstr(mem->serial_log, "Failed"))
    {
        mem->test_done = 1;
    }
}

void mem_init(Mem *mem, const Cart *cart)
{
    memset(mem, 0, sizeof(*mem));

    mem->cart = cart;

    cartridge_init(&mem->cartridge, cart);

    if (cart && cart->data && cart->size >= 0x150)
        mem->cgb = (cart->data[0x0143] & 0x80) != 0;

    for (int i = 0; i < 64; i += 2)
    {
        mem->bg_pal[i] = mem->obj_pal[i] = 0xFF;
        mem->bg_pal[i + 1] = mem->obj_pal[i + 1] = 0x7F;
    }

    mem->io[0x40] = 0x91;
    mem->io[0x47] = 0xFC;
    mem->io[0x48] = 0xFF;
    mem->io[0x49] = 0xFF;
}

uint8_t mem_read(const Mem *mem, uint16_t addr)
{
    if (addr < 0x8000)
        return cartridge_read(&mem->cartridge, addr);

    if (addr < 0xA000)
        return mem->vram[vram_bank(mem)][addr - 0x8000];

    if (addr < 0xC000)
        return cartridge_read(&mem->cartridge, addr);

    if (addr < 0xFE00)
    {
        uint16_t offset = addr & 0x1FFF;

        return offset < 0x1000
                   ? mem->wram[0][offset]
                   : mem->wram[wram_bank(mem)][offset - 0x1000];
    }

    if (addr < 0xFEA0)
        return mem->oam[addr - 0xFE00];

    if (addr < 0xFF00)
        return 0xFF;

    if (addr < 0xFF80)
    {
        switch (addr)
        {
        case 0xFF00:
            return joypad_read(mem);

        case 0xFF04:
            return (uint8_t)(mem->div_counter >> 8);

        case 0xFF07:
            return mem->io[0x07] | 0xF8;

        case 0xFF0F:
            return mem->io[0x0F] | 0xE0;

        case 0xFF41:
            return mem->io[0x41] | 0x80;

        case 0xFF44:
            return mem->ly_stub ? 0x90 : mem->io[0x44];

        case 0xFF4D:
            return mem->cgb
                       ? (uint8_t)(0x7E | (mem->double_speed << 7) | (mem->key1 & 1))
                       : 0xFF;

        case 0xFF4F:
            return mem->cgb ? (uint8_t)(0xFE | (mem->vbk & 1)) : 0xFF;

        case 0xFF70:
            return mem->cgb ? (uint8_t)(0xF8 | (mem->svbk & 7)) : 0xFF;

        case 0xFF68:
            return mem->cgb ? (uint8_t)(0x40 | mem->bcps) : 0xFF;

        case 0xFF69:
            return mem->cgb ? mem->bg_pal[mem->bcps & 0x3F] : 0xFF;

        case 0xFF6A:
            return mem->cgb ? (uint8_t)(0x40 | mem->ocps) : 0xFF;

        case 0xFF6B:
            return mem->cgb ? mem->obj_pal[mem->ocps & 0x3F] : 0xFF;

        case 0xFF6C:
            return mem->cgb ? (uint8_t)(0xFE | (mem->opri & 1)) : 0xFF;

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
    {
        cartridge_write(&mem->cartridge, addr, val);
        return;
    }

    if (addr < 0xA000)
    {
        mem->vram[vram_bank(mem)][addr - 0x8000] = val;
        return;
    }

    if (addr < 0xC000)
    {
        cartridge_write(&mem->cartridge, addr, val);
        return;
    }

    if (addr < 0xFE00)
    {
        uint16_t offset = addr & 0x1FFF;

        if (offset < 0x1000)
            mem->wram[0][offset] = val;
        else
            mem->wram[wram_bank(mem)][offset - 0x1000] = val;

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
        case 0xFF00:
            joypad_write(mem, val);
            return;

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

        case 0xFF07:
            mem->io[0x07] = val & 7;
            timer_sync(mem);
            return;

        case 0xFF41:
            mem->io[0x41] =
                (uint8_t)((mem->io[0x41] & 0x07) | (val & 0x78));
            return;

        case 0xFF44:
            return;

        case 0xFF46:
        {
            uint16_t source = (uint16_t)(val << 8);

            for (int i = 0; i < 0xA0; i++)
                mem->oam[i] = mem_read(mem, (uint16_t)(source + i));

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

        case 0xFF68:
            if (mem->cgb)
                mem->bcps = val & 0xBF;
            return;

        case 0xFF6A:
            if (mem->cgb)
                mem->ocps = val & 0xBF;
            return;

        case 0xFF69:
            if (mem->cgb)
            {
                mem->bg_pal[mem->bcps & 0x3F] = val;

                if (mem->bcps & 0x80)
                    mem->bcps = (uint8_t)((mem->bcps & 0x80) |
                                          ((mem->bcps + 1) & 0x3F));
            }
            return;

        case 0xFF6B:
            if (mem->cgb)
            {
                mem->obj_pal[mem->ocps & 0x3F] = val;

                if (mem->ocps & 0x80)
                    mem->ocps = (uint8_t)((mem->ocps & 0x80) |
                                          ((mem->ocps + 1) & 0x3F));
            }
            return;

        case 0xFF6C:
            if (mem->cgb)
                mem->opri = val & 1;
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