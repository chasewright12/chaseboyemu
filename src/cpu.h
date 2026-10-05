#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "mem.h"

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

typedef struct {
    uint8_t a, f, b, c, d, e, h, l;
    uint16_t sp, pc;
    uint8_t ime; // Interrupt Master Enable
    uint8_t ei_pending;
    uint8_t halted;
    uint8_t halt_bug;
    uint64_t cycles; // Clocks cicle (T-states)

    Mem *mem;
} Cpu;

void cpu_init(Cpu *cpu, Mem *mem);
int cpu_step(Cpu *cpu);
void cpu_log(const Cpu *cpu);
void cpu_log_doctor(const Cpu *cpu);

#endif