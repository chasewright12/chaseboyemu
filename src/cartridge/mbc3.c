#include "mbc3.h"

static size_t mbc3_rom_bank_count(const Cartridge *cartridge)
{
    return (cartridge->rom->size + 0x3FFF) / 0x4000;
}

static size_t mbc3_ram_bank_count(const Cartridge *cartridge)
{
    if (cartridge->ram_size == 0)
        return 0;

    return (cartridge->ram_size + 0x1FFF) / 0x2000;
}

static size_t mbc3_ram_offset(const Cartridge *cartridge, uint16_t addr)
{
    if (!cartridge->ram_enabled || cartridge->ram_size == 0)
        return SIZE_MAX;

    if (cartridge->mbc3_ram_rtc_select > 3)
        return SIZE_MAX;

    size_t bank_count = mbc3_ram_bank_count(cartridge);

    if (bank_count == 0)
        return SIZE_MAX;

    size_t bank = cartridge->mbc3_ram_rtc_select % bank_count;
    size_t offset = bank * 0x2000 + (addr - 0xA000);

    return offset < cartridge->ram_size ? offset : SIZE_MAX;
}

uint8_t mbc3_read(const Cartridge *cartridge, uint16_t addr)
{
    if (addr < 0x4000)
    {
        if (addr >= cartridge->rom->size)
            return 0xFF;

        return cartridge->rom->data[addr];
    }

    if (addr < 0x8000)
    {
        size_t bank_count = mbc3_rom_bank_count(cartridge);

        if (bank_count == 0)
            return 0xFF;

        size_t bank = cartridge->mbc3_rom_bank & 0x7F;

        if (bank == 0)
            bank = 1;

        bank %= bank_count;

        size_t offset = bank * 0x4000 + (addr & 0x3FFF);

        if (offset >= cartridge->rom->size)
            return 0xFF;

        return cartridge->rom->data[offset];
    }

    if (addr >= 0xA000 && addr < 0xC000)
    {
        if (!cartridge->ram_enabled)
            return 0xFF;

        if (cartridge->mbc3_ram_rtc_select <= 3)
        {
            size_t offset = mbc3_ram_offset(cartridge, addr);

            if (offset == SIZE_MAX)
                return 0xFF;

            return cartridge->ram[offset];
        }

        if (cartridge->mbc3_has_rtc &&
            cartridge->mbc3_ram_rtc_select >= 0x08 &&
            cartridge->mbc3_ram_rtc_select <= 0x0C)
        {
            return cartridge->mbc3_rtc_latched[cartridge->mbc3_ram_rtc_select - 0x08];
        }
    }

    return 0xFF;
}

void mbc3_write(Cartridge *cartridge, uint16_t addr, uint8_t value)
{
    if (addr < 0x2000)
    {
        cartridge->ram_enabled = (value & 0x0F) == 0x0A;
    }
    else if (addr < 0x4000)
    {
        cartridge->mbc3_rom_bank = value & 0x7F;

        if (cartridge->mbc3_rom_bank == 0)
            cartridge->mbc3_rom_bank = 1;
    }
    else if (addr < 0x6000)
    {
        cartridge->mbc3_ram_rtc_select = value;
    }
    else if (addr < 0x8000)
    {
        if (cartridge->mbc3_latch == 0 && value == 1)
        {
            for (int i = 0; i < 5; i++)
                cartridge->mbc3_rtc_latched[i] = cartridge->mbc3_rtc[i];
        }

        cartridge->mbc3_latch = value;
    }
    else if (addr >= 0xA000 && addr < 0xC000)
    {
        if (!cartridge->ram_enabled)
            return;

        if (cartridge->mbc3_ram_rtc_select <= 3)
        {
            size_t offset = mbc3_ram_offset(cartridge, addr);

            if (offset != SIZE_MAX)
                cartridge->ram[offset] = value;
        }
        else if (cartridge->mbc3_has_rtc &&
                 cartridge->mbc3_ram_rtc_select >= 0x08 &&
                 cartridge->mbc3_ram_rtc_select <= 0x0C)
        {
            cartridge->mbc3_rtc[cartridge->mbc3_ram_rtc_select - 0x08] = value;
        }
    }
}