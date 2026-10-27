#ifndef JOYPAD_H
#define JOYPAD_H

#include <stdint.h>
#include "mem.h"

/* The enum values are the bit positions in mem->joy_state */
typedef enum
{
    BTN_RIGHT,
    BTN_LEFT,
    BTN_UP,
    BTN_DOWN,
    BTN_A,
    BTN_B,
    BTN_SELECT,
    BTN_START
} Button;

/* Register 0xFF00 (P1): bits 4-5 choose which group the CPU reads, and bits 0-3
   return that group's buttons, where 0 means pressed. */
uint8_t joypad_read(const Mem *mem);
void joypad_write(Mem *mem, uint8_t val);

void joypad_set(Mem *mem, Button button, int pressed);

#endif