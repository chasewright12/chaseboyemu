#include "hdma.h"

#include <string.h>

#define HDMA1 0xFF51
#define HDMA2 0xFF52
#define HDMA3 0xFF53
#define HDMA4 0xFF54
#define HDMA5 0xFF55

#define VRAM_START 0x8000
#define VRAM_END 0xA000

#define HDMA_BLOCK_SIZE 16

static uint16_t get_source(Hdma *hdma)
{
    uint8_t high = hdma->mem->io[HDMA1 - 0xFF00];
    uint8_t low = hdma->mem->io[HDMA2 - 0xFF00] & 0xF0;

    return ((uint16_t)high << 8) | low;
}

static uint16_t get_destination(Hdma *hdma)
{
    uint8_t high = hdma->mem->io[HDMA3 - 0xFF00] & 0x1F;
    uint8_t low = hdma->mem->io[HDMA4 - 0xFF00] & 0xF0;

    return VRAM_START | ((uint16_t)high << 8) | low;
}

static void update_registers(Hdma *hdma)
{
    hdma->mem->io[HDMA1 - 0xFF00] =
        (uint8_t)(hdma->source >> 8);
    hdma->mem->io[HDMA2 - 0xFF00] =
        (uint8_t)(hdma->source & 0xF0);
    hdma->mem->io[HDMA3 - 0xFF00] =
        (uint8_t)((hdma->destination >> 8) & 0x1F);

    hdma->mem->io[HDMA4 - 0xFF00] =
        (uint8_t)(hdma->destination & 0xF0);
}

void hdma_init(Hdma *hdma, Mem *mem)
{
    memset(hdma, 0, sizeof(*hdma));

    hdma->mem = mem;
    hdma->stall_dots = 0;
}

uint8_t hdma_read_register(const Hdma *hdma, uint16_t addr) {
    if (!hdma || !hdma->mem || !hdma->mem->cgb)
        return 0xFF;

    if (addr != HDMA5)
        return 0xFF;

    if (hdma->active) {
        return (uint8_t)((hdma->blocks_remaining - 1) & 0x7F);
    }
    if (hdma->cancelled) {
        return (uint8_t)(0x80 | ((hdma->blocks_remaining - 1) & 0x7F));
    }

    return 0xFF;
}

void hdma_write_register(Hdma *hdma, uint16_t addr, uint8_t value)
{
    if (!hdma || !hdma->mem || !hdma->mem->cgb)
        return;

    switch (addr) {
        case HDMA1:
            if (!hdma->active)
                hdma->mem->io[addr - 0xFF00] = value;
            break;
        
        case HDMA2:
            if (!hdma->active)
                hdma->mem->io[addr - 0xFF00] = value & 0xF0;
            break;

        case HDMA3:
            if (!hdma->active)
                hdma->mem->io[addr - 0xFF00] = value & 0x1F;
            break;

        case HDMA4:
            if (!hdma->active)
                hdma->mem->io[addr - 0xFF00] = value & 0xF0;
            break;

        case HDMA5:
            hdma_start(hdma, value);
            break;

        default:
            break;
    }
}

void hdma_start(Hdma *hdma, uint8_t value)
{
    if (!hdma || !hdma->mem || !hdma->mem->cgb)
        return;

    if (hdma->active && hdma->hblank_mode)
    {
        if (!(value & 0x80))
        {
            hdma->cancelled = 1;
            hdma->active = 0;
        }

        return;
    }

    if (hdma->active)
        return;

    hdma->source = get_source(hdma);
    hdma->destination = get_destination(hdma);

    hdma->blocks_remaining = (value & 0x7F) + 1;

    hdma->active = 1;
    hdma->cancelled = 0;
    hdma->hblank_mode = (value & 0x80) != 0;

    /* LCD disabled: HDMA behaves as GDMA. */
    if (!(hdma->mem->io[0x40] & 0x80))
        hdma->hblank_mode = 0;

    if (!hdma->hblank_mode)
    {
        while (hdma->active)
            hdma_transfer_block(hdma);
    }
}

static uint8_t hdma_read_source(Hdma *hdma, uint16_t addr)
{
    return mem_read(hdma->mem, addr);
}

static void hdma_write_vram(Hdma *hdma, uint16_t addr, uint8_t value)
{
    unsigned bank = hdma->mem->vbk & 1;

    if (addr < VRAM_START || addr >= VRAM_END)
        return;

    hdma->mem->vram[bank][addr - VRAM_START] = value;
}

void hdma_transfer_block(Hdma *hdma)
{
    if (!hdma || !hdma->mem || !hdma->active)
        return;

    if (hdma->blocks_remaining == 0) {
        hdma->active = 0;
        return;
    }

    for (int i = 0; i < HDMA_BLOCK_SIZE; i++) {
        uint8_t value = hdma_read_source(hdma, hdma->source);

        hdma_write_vram(hdma, hdma->destination, value);

        hdma->source++;
        hdma->destination++;

        if (hdma->destination >= VRAM_END)
            hdma->destination = VRAM_START;
    }

    hdma->blocks_remaining--;

    update_registers(hdma);
    /* Stall CPU for the duration of one DMA block. */
    hdma->stall_dots += HDMA_BLOCK_DOTS;

    if (hdma->blocks_remaining == 0) {
        hdma->active = 0;
        hdma->hblank_mode = 0;
        hdma->cancelled = 0;
    }
    else if (hdma->cancelled) {
        hdma->active = 0;
        hdma->hblank_mode = 0;
    }
}

void hdma_hblank(Hdma *hdma)
{
    if (!hdma || !hdma->active || !hdma->hblank_mode)
        return;

    hdma_transfer_block(hdma);
}

int hdma_is_active(const Hdma *hdma)
{
    return hdma && hdma->active;
}

int hdma_cpu_stalled(const Hdma *hdma)
{
    if (!hdma)
        return 0;

    return hdma->stall_dots > 0;
}

void hdma_tick(Hdma *hdma, int dots)
{
    if (!hdma || dots <= 0)
        return;

    if ((uint32_t)dots >= hdma->stall_dots)
    {
        hdma->stall_dots = 0;
    }
    else
    {
        hdma->stall_dots -= (uint32_t)dots;
    }
}