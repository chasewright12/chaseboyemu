#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stddef.h>

#include "../cart.h"

#define CARTRIDGE_MAX_RAM_SIZE 0x20000

typedef enum
{
    CART_MBC_NONE = 0,
    CART_MBC1,
    CART_MBC3,
    CART_MBC5
} CartMbc;

typedef struct
{
    const Cart *rom;

    CartMbc mbc;
    uint8_t type;

    uint8_t ram_enabled;

    uint8_t mbc1_rom_low5;
    uint8_t mbc1_bank_high2;
    uint8_t mbc1_mode;

    uint8_t mbc3_rom_bank;
    uint8_t mbc3_ram_rtc_select;
    uint8_t mbc3_latch;
    uint8_t mbc3_rtc[5];
    uint8_t mbc3_rtc_latched[5];
    uint8_t mbc3_has_rtc;

    uint16_t mbc5_rom_bank;
    uint8_t mbc5_ram_bank;
    uint8_t mbc5_rumble;

    uint8_t ram[CARTRIDGE_MAX_RAM_SIZE];
    size_t ram_size;

} Cartridge;

int cartridge_init(Cartridge *cartridge, const Cart *rom);

uint8_t cartridge_read(const Cartridge *cartridge, uint16_t addr);

void cartridge_write(Cartridge *cartridge, uint16_t addr, uint8_t value);

#endif