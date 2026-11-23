#ifndef MBC1_H
#define MBC1_H

#include "cartridge.h"

uint8_t mbc1_read(const Cartridge *cartridge, uint16_t addr);

void mbc1_write(Cartridge *cartridge, uint16_t addr, uint8_t value);

#endif