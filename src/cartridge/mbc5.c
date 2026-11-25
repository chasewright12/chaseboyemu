#include "mbc5.h"

static size_t mbc5_rom_bank_count(const Cartridge *cartridge)
{
    if (cartridge->rom->size == 0)
        return 0;

    return (cartridge->rom->size + 0x3FFF) / 0x4000;
}

static size_t mbc5_ram_bank_count(const Cartridge *cartridge)
{
    if (cartridge->ram_size == 0)
        return 0;

    return (cartridge->ram_size + 0x1FFF) / 0x2000;
}

static size_t mbc5_ram_offset(const Cartridge *cartridge, uint16_t addr)
{
    if (!cartridge->ram_enabled || cartridge->ram_size == 0)
        return SIZE_MAX;

    size_t bank_count = mbc5_ram_bank_count(cartridge);

    if (bank_count == 0)
        return SIZE_MAX;

    size_t bank = cartridge->mbc5_ram_bank % bank_count;
    size_t offset = bank * 0x2000 + (addr - 0xA000);

    return offset < cartridge->ram_size ? offset : SIZE_MAX;
}

uint8_t mbc5_read(const Cartridge *cartridge, uint16_t addr)
{
    if (addr < 0x8000)
    {
        size_t bank;
        size_t offset;

        if (addr < 0x4000)
        {
            bank = 0;
            offset = addr;
        }
        else
        {
            bank = cartridge->mbc5_rom_bank & 0x1FF;
            offset = bank * 0x4000 + (addr & 0x3FFF);
        }

        size_t bank_count = mbc5_rom_bank_count(cartridge);

        if (bank >= bank_count || offset >= cartridge->rom->size)
            return 0xFF;

        return cartridge->rom->data[offset];
    }

    if (addr >= 0xA000 && addr < 0xC000)
    {
        size_t offset = mbc5_ram_offset(cartridge, addr);

        if (offset == SIZE_MAX)
            return 0xFF;

        return cartridge->ram[offset];
    }

    return 0xFF;
}

void mbc5_write(Cartridge *cartridge, uint16_t addr, uint8_t value)
{
    if (addr < 0x2000)
    {
        cartridge->ram_enabled = (value & 0x0F) == 0x0A;
    }
    else if (addr < 0x3000)
    {
        cartridge->mbc5_rom_bank =
            (uint16_t)((cartridge->mbc5_rom_bank & 0x100) | value);
    }
    else if (addr < 0x4000)
    {
        cartridge->mbc5_rom_bank =
            (uint16_t)((cartridge->mbc5_rom_bank & 0x0FF) |
                       ((value & 0x01) << 8));
    }
    else if (addr < 0x6000)
    {
        if (cartridge->mbc5_rumble)
        {
            cartridge->mbc5_ram_bank = value & 0x07;
        }
        else
        {
            cartridge->mbc5_ram_bank = value & 0x0F;
        }
    }
    else if (addr >= 0xA000 && addr < 0xC000)
    {
        size_t offset = mbc5_ram_offset(cartridge, addr);

        if (offset != SIZE_MAX)
            cartridge->ram[offset] = value;
    }
}