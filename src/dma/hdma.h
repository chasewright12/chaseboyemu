#ifndef HDMA_H
#define HDMA_H

#include <stdint.h>
#include "../mem.h"

#define HDMA_BLOCK_SIZE 16
#define HDMA_BLOCK_DOTS 32
#define HDMA_BYTE_DOTS 2

struct Hdma
{
    Mem *mem;

    uint16_t source;
    uint16_t destination;

    uint8_t blocks_remaining;

    uint8_t active;
    uint8_t hblank_mode;
    uint8_t cancelled;

    /* Progressive transfer state. */
    uint8_t block_active;
    uint8_t bytes_transferred;
    uint8_t dot_counter;
    uint8_t vram_bank;

    /* Remaining CPU stall duration in PPU dots. */
    uint32_t stall_dots;
};

void hdma_init(Hdma *hdma, Mem *mem);

uint8_t hdma_read_register(
    const Hdma *hdma,
    uint16_t addr
);

void hdma_write_register(
    Hdma *hdma,
    uint16_t addr,
    uint8_t value
);

void hdma_start(Hdma *hdma, uint8_t value);
void hdma_transfer_block(Hdma *hdma);
void hdma_hblank(Hdma *hdma);

int hdma_is_active(const Hdma *hdma);
int hdma_cpu_stalled(const Hdma *hdma);

// DMA Progressive Execution.
void hdma_tick(Hdma *hdma, int dots);

#endif