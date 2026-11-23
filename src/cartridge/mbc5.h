#ifndef MBC5_H
#define MBC5_H

#include "cartridge.h"

uint8_t mbc5_read(const Cartridge *cartridge, uint16_t addr);

void mbc5_write(Cartridge *cartridge, uint16_t addr, uint8_t value);

#endif