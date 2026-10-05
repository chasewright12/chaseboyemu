#include "cart.h"
#include "mem.h"
#include "cpu.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

#define MAX_CYCLES 1000000000ULL   /* limite de seguranca (~4 minutos de Game Boy) */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <rom.gb> [--doctor]\n", argv[0]);
        return 1;
    }
    int doctor = argc > 2 && strcmp(argv[2], "--doctor") == 0;

    Cart cart;
    if (!cart_load(&cart, argv[1])) {
        return 1;
    }
    if (!doctor) {
        cart_print_header(&cart);
    }

    Mem mem;
    mem_init(&mem, &cart);

    Cpu cpu;
    cpu_init(&cpu, &mem);

    while (!mem.test_done && cpu.cycles < MAX_CYCLES) {
        if (doctor) {
            cpu_log_doctor(&cpu);
        }
        int cycles = cpu_step(&cpu);
        if (cycles < 0) {
            break;
        }
        timer_tick(&mem, cycles);   /* os outros componentes acompanham o tempo da CPU */
    }

    if (!doctor) {
        fprintf(stderr, "\nCiclos executados: %llu\n", (unsigned long long)cpu.cycles);
    }

    cart_free(&cart);
    return 0;
}
