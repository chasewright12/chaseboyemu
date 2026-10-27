#include "joypad.h"

#define IRQ_JOYPAD 0x10

/* Low nibble of P1 as the CPU would read it right now: 1 = released, 0 = pressed. */
static uint8_t lines(const Mem *mem)
{
    uint8_t keys = 0x0F;
    if (!(mem->joy_select & 0x10))
        keys &= (uint8_t)~(mem->joy_state & 0x0F);
    if (!(mem->joy_select & 0x20))
        keys &= (uint8_t)~((mem->joy_state >> 4) & 0x0F);
    return keys;
}

/* A line going from 1 to 0 requests the joypad interrupt. */
static void request_on_falling_edge(Mem *mem, uint8_t before)
{
    if (before & (uint8_t)~lines(mem))
    {
        mem->io[0x0F] |= IRQ_JOYPAD;
    }
}

uint8_t joypad_read(const Mem *mem)
{
    return (uint8_t)(0xC0 | mem->joy_select | lines(mem));
}

void joypad_write(Mem *mem, uint8_t val)
{
    uint8_t before = lines(mem);
    mem->joy_select = val & 0x30;
    request_on_falling_edge(mem, before);
}

void joypad_set(Mem *mem, Button button, int pressed)
{
    uint8_t before = lines(mem);
    if (pressed)
    {
        mem->joy_state |= (uint8_t)(1 << button);
    }
    else
    {
        mem->joy_state &= (uint8_t)~(1 << button);
    }
    request_on_falling_edge(mem, before);
}