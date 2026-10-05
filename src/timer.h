#ifndef TIMER_H
#define TIMER_H

#include "mem.h"

void timer_tick(Mem *mem, int cycles);

void timer_sync(Mem *mem);

#endif