#include "mbc1.h"

static size_t mbc1_rom_bank_count(const Cartridge *cartridge)
{
    return (cartridge->rom->size + 0x3FFF) / 0x4000;
}

static size_t mbc1_ram_bank_count(const Cartridge *cartridge)
{
    if (cartridge->ram_size == 0)
        return 0;

    return (cartridge->ram_size + 0x1FFF) / 0x2000;
}

static size_t mbc1_rom_offset(const Cartridge *cartridge, uint16_t addr)
{
    size_t bank;

    if (addr < 0x4000)
    {
        bank = cartridge->mbc1_mode
                   ? ((size_t)cartridge->mbc1_bank_high2 << 5)
                   : 0;
    }
    else
    {
        uint8_t low = cartridge->mbc1_rom_low5 & 0x1F;

        if (low == 0)
            low = 1;

        bank = ((size_t)cartridge->mbc1_bank_high2 << 5) | low;
    }

    size_t bank_count = mbc1_rom_bank_count(cartridge);

    if (bank_count == 0)
        return SIZE_MAX;

    bank %= bank_count;

    return bank * 0x4000 + (addr & 0x3FFF);
}

static size_t mbc1_ram_offset(const Cartridge *cartridge, uint16_t addr)
{
    if (!cartridge->ram_enabled || cartridge->ram_size == 0)
        return SIZE_MAX;

    size_t bank_count = mbc1_ram_bank_count(cartridge);

    if (bank_count == 0)
        return SIZE_MAX;

    size_t bank = cartridge->mbc1_mode
                      ? cartridge->mbc1_bank_high2
                      : 0;

    bank %= bank_count;

    size_t offset = addr - 0xA000;

    if (cartridge->ram_size == 0x0800)
        offset &= 0x07FF;

    size_t index = bank * 0x2000 + offset;

    return index < cartridge->ram_size ? index : SIZE_MAX;
}

uint8_t mbc1_read(const Cartridge *cartridge, uint16_t addr)
{
    if (addr < 0x8000)
    {
        size_t offset = mbc1_rom_offset(cartridge, addr);

        if (offset == SIZE_MAX || offset >= cartridge->rom->size)
            return 0xFF;

        return cartridge->rom->data[offset];
    }

    if (addr >= 0xA000 && addr < 0xC000)
    {
        size_t offset = mbc1_ram_offset(cartridge, addr);

        if (offset == SIZE_MAX)
            return 0xFF;

        return cartridge->ram[offset];
    }

    return 0xFF;
}

void mbc1_write(Cartridge *cartridge, uint16_t addr, uint8_t value)
{
    if (addr < 0x2000)
    {
        cartridge->ram_enabled = (value & 0x0F) == 0x0A;
    }
    else if (addr < 0x4000)
    {
        cartridge->mbc1_rom_low5 = value & 0x1F;
    }
    else if (addr < 0x6000)
    {
        cartridge->mbc1_bank_high2 = value & 0x03;
    }
    else if (addr < 0x8000)
    {
        cartridge->mbc1_mode = value & 0x01;
    }
    else if (addr >= 0xA000 && addr < 0xC000)
    {
        size_t offset = mbc1_ram_offset(cartridge, addr);

        if (offset != SIZE_MAX)
            cartridge->ram[offset] = value;
    }
}