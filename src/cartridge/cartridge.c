#include "cartridge.h"

#include "mbc1.h"
#include "mbc3.h"
#include "mbc5.h"

#include <string.h>

static CartMbc cartridge_get_mbc(uint8_t type)
{
    switch (type)
    {
    case 0x01:
    case 0x02:
    case 0x03:
        return CART_MBC1;

    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
        return CART_MBC3;

    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
        return CART_MBC5;

    default:
        return CART_MBC_NONE;
    }
}

static size_t cartridge_get_ram_size(const Cart *rom)
{
    if (!rom || !rom->data || rom->size < 0x150)
        return 0;

    switch (rom->data[0x0149])
    {
    case 0x01:
        return 0x0800;

    case 0x02:
        return 0x2000;

    case 0x03:
        return 0x8000;

    case 0x04:
        return 0x20000;

    case 0x05:
        return 0x10000;

    default:
        return 0;
    }
}

int cartridge_init(Cartridge *cartridge, const Cart *rom)
{
    if (!cartridge || !rom || !rom->data || rom->size < 0x150)
        return -1;

    memset(cartridge, 0, sizeof(*cartridge));

    cartridge->rom = rom;
    cartridge->type = rom->data[0x0147];
    cartridge->mbc = cartridge_get_mbc(cartridge->type);

    cartridge->ram_size = cartridge_get_ram_size(rom);

    if (cartridge->ram_size > CARTRIDGE_MAX_RAM_SIZE)
        cartridge->ram_size = CARTRIDGE_MAX_RAM_SIZE;

    cartridge->mbc1_rom_low5 = 1;
    cartridge->mbc3_rom_bank = 1;
    cartridge->mbc5_rom_bank = 1;

    switch (cartridge->type)
    {
    case 0x0F:
    case 0x10:
        cartridge->mbc3_has_rtc = 1;
        break;

    default:
        cartridge->mbc3_has_rtc = 0;
        break;
    }

    switch (cartridge->type)
    {
    case 0x1C:
    case 0x1D:
    case 0x1E:
        cartridge->mbc5_rumble = 1;
        break;

    default:
        cartridge->mbc5_rumble = 0;
        break;
    }

    return 0;
}

uint8_t cartridge_read(const Cartridge *cartridge, uint16_t addr)
{
    if (!cartridge || !cartridge->rom || !cartridge->rom->data)
        return 0xFF;

    switch (cartridge->mbc)
    {
    case CART_MBC1:
        return mbc1_read(cartridge, addr);

    case CART_MBC3:
        return mbc3_read(cartridge, addr);

    case CART_MBC5:
        return mbc5_read(cartridge, addr);

    case CART_MBC_NONE:
    default:
        if (addr < cartridge->rom->size)
            return cartridge->rom->data[addr];

        if (addr >= 0xA000 && addr < 0xC000)
        {
            size_t offset = addr - 0xA000;

            if (offset < cartridge->ram_size)
                return cartridge->ram[offset];
        }

        return 0xFF;
    }
}

void cartridge_write(Cartridge *cartridge, uint16_t addr, uint8_t value)
{
    if (!cartridge || !cartridge->rom)
        return;

    switch (cartridge->mbc)
    {
    case CART_MBC1:
        mbc1_write(cartridge, addr, value);
        break;

    case CART_MBC3:
        mbc3_write(cartridge, addr, value);
        break;

    case CART_MBC5:
        mbc5_write(cartridge, addr, value);
        break;

    case CART_MBC_NONE:
    default:
        if (addr >= 0xA000 && addr < 0xC000)
        {
            size_t offset = addr - 0xA000;

            if (offset < cartridge->ram_size)
                cartridge->ram[offset] = value;
        }
        break;
    }
}