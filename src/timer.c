#include "timer.h"

#define REG_TIMA 0x05
#define REG_TMA  0x06
#define REG_TAC  0x07
#define REG_IF   0x0F

#define TAC_ENABLE 0x04
#define IRQ_TIMER  0x04

/* 
   00 -> bit 9 (4096 Hz), 01 -> bit 3 (262144 Hz), 10 -> bit 5 (65536 Hz), 11 -> bit 7 (16384 Hz) */
static const int selected_bit[4] = { 9, 3, 5, 7 };

static void tima_increment(Mem *mem) {
    if (++mem->io[REG_TIMA] == 0) {
        mem->io[REG_TIMA] = mem->io[REG_TMA];
        mem->io[REG_IF] |= IRQ_TIMER;
    }
}

void timer_sync(Mem *mem) {
    uint8_t tac = mem->io[REG_TAC];
    int signal = (tac & TAC_ENABLE) && ((mem->div_counter >> selected_bit[tac & 3]) & 1);

    if (mem->timer_signal && !signal) {
        tima_increment(mem);
    }
    mem->timer_signal = (uint8_t)signal;
}

void timer_tick(Mem *mem, int cycles) {
    for (int i = 0; i < cycles; i++) {
        mem->div_counter++;
        timer_sync(mem);
    }
}