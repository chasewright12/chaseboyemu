#ifndef MBC3_H
#define MBC3_H

#include "cartridge.h"

uint8_t mbc3_read(const Cartridge *cartridge, uint16_t addr);

void mbc3_write(Cartridge *cartridge, uint16_t addr, uint8_t value);

#endif