
#include "hdma.h"
#include <string.h>

/*
 * Update the internal source and destination registers
 * after completing a DMA block.
 */
static void hdma_update_registers(Hdma *hdma)
{
    Mem *mem = hdma->mem;

    mem->io[0x51] =
        (uint8_t)(hdma->source >> 8);

    mem->io[0x52] =
        (uint8_t)(hdma->source & 0xF0);

    mem->io[0x53] =
        (uint8_t)((hdma->destination >> 8) & 0x1F);

    mem->io[0x54] =
        (uint8_t)(hdma->destination & 0xF0);
}

/*
 * Finish the current DMA operation.
 */
static void hdma_finish(Hdma *hdma)
{
    hdma->active = 0;
    hdma->block_active = 0;
    hdma->cancelled = 0;

    hdma->blocks_remaining = 0;
    hdma->bytes_transferred = 0;
    hdma->dot_counter = 0;
    hdma->stall_dots = 0;

    hdma->mem->io[0x55] = 0xFF;
}

/*
 * Initialize the DMA controller.
 */
void hdma_init(Hdma *hdma, Mem *mem)
{
    memset(hdma, 0, sizeof(*hdma));

    hdma->mem = mem;
    hdma->destination = 0x8000;

    mem->io[0x55] = 0xFF;
}

/*
 * Read HDMA registers.
 */
uint8_t hdma_read_register(
    const Hdma *hdma,
    uint16_t addr)
{
    if (!hdma || !hdma->mem)
        return 0xFF;

    switch (addr)
    {
    case 0xFF51:
    case 0xFF52:
    case 0xFF53:
    case 0xFF54:
        return hdma->mem->io[addr - 0xFF00];

    case 0xFF55:

        if (hdma->active &&
            hdma->blocks_remaining > 0)
        {
            return (uint8_t)((hdma->blocks_remaining - 1) & 0x7F);
        }

        if (hdma->cancelled &&
            hdma->blocks_remaining > 0)
        {
            return (uint8_t)(0x80 |
                             ((hdma->blocks_remaining - 1) & 0x7F));
        }

        return 0xFF;

    default:
        return 0xFF;
    }
}

/*
 * Write HDMA registers.
 */
void hdma_write_register(
    Hdma *hdma,
    uint16_t addr,
    uint8_t value)
{
    if (!hdma || !hdma->mem)
        return;

    if (!hdma->mem->cgb)
        return;

    /*
     * Ignore source and destination register
     * modifications during an active transfer.
     */
    if (hdma->active && addr != 0xFF55)
        return;

    switch (addr)
    {
    case 0xFF51:
        hdma->mem->io[0x51] = value;
        break;

    case 0xFF52:
        hdma->mem->io[0x52] = value & 0xF0;
        break;

    case 0xFF53:
        hdma->mem->io[0x53] = value & 0x1F;
        break;

    case 0xFF54:
        hdma->mem->io[0x54] = value & 0xF0;
        break;

    case 0xFF55:
        hdma_start(hdma, value);
        break;

    default:
        break;
    }
}

/*
 * Schedule a new 16-byte DMA block.
 *
 * Unlike the previous implementation,
 * this function does not copy bytes immediately.
 */
void hdma_transfer_block(Hdma *hdma)
{
    if (!hdma || !hdma->active)
        return;

    if (hdma->block_active)
        return;

    if (hdma->blocks_remaining == 0)
        return;

    hdma->block_active = 1;
    hdma->bytes_transferred = 0;
    hdma->dot_counter = 0;

    hdma->vram_bank = hdma->mem->vbk & 1;

    hdma->stall_dots = HDMA_BLOCK_DOTS;
}

/*
 * Start a GDMA or HBlank DMA operation.
 */
void hdma_start(Hdma *hdma, uint8_t value)
{
    if (!hdma || !hdma->mem)
        return;

    if (!hdma->mem->cgb)
        return;

    /*
     * Cancel an active HBlank DMA transfer
     * between blocks.
     */
    if (hdma->active)
    {
        if (hdma->hblank_mode &&
            !(value & 0x80) &&
            !hdma->block_active)
        {
            hdma->active = 0;
            hdma->cancelled = 1;
            hdma->stall_dots = 0;

            hdma->mem->io[0x55] =
                hdma_read_register(hdma, 0xFF55);
        }

        return;
    }

    Mem *mem = hdma->mem;

    hdma->source = (uint16_t)(((uint16_t)mem->io[0x51] << 8) |
                              (mem->io[0x52] & 0xF0));

    hdma->destination = (uint16_t)(0x8000 |
                                   ((uint16_t)(mem->io[0x53] & 0x1F) << 8) |
                                   (mem->io[0x54] & 0xF0));

    hdma->blocks_remaining =
        (uint8_t)((value & 0x7F) + 1);

    hdma->hblank_mode =
        (value & 0x80) != 0;

    hdma->active = 1;
    hdma->cancelled = 0;
    hdma->block_active = 0;

    hdma->bytes_transferred = 0;
    hdma->dot_counter = 0;
    hdma->stall_dots = 0;

    /*
     * Simplified LCD-disabled behavior:
     * fall back to General Purpose DMA.
     */
    if (!(mem->io[0x40] & 0x80))
        hdma->hblank_mode = 0;

    mem->io[0x55] =
        hdma_read_register(hdma, 0xFF55);

    /*
     * GDMA begins immediately, but bytes are
     * transferred only when hdma_tick() runs.
     */
    if (!hdma->hblank_mode)
        hdma_transfer_block(hdma);
}

/*
 * Called when the PPU enters HBlank.
 */
void hdma_hblank(Hdma *hdma)
{
    if (!hdma || !hdma->active)
        return;

    if (!hdma->hblank_mode)
        return;

    if (hdma->block_active)
        return;

    hdma_transfer_block(hdma);
}

/*
 * Complete a 16-byte block.
 */
static void hdma_complete_block(Hdma *hdma)
{
    hdma->block_active = 0;
    hdma->bytes_transferred = 0;
    hdma->dot_counter = 0;
    hdma->stall_dots = 0;

    if (hdma->blocks_remaining > 0)
        hdma->blocks_remaining--;

    hdma_update_registers(hdma);

    if (hdma->blocks_remaining == 0)
    {
        hdma_finish(hdma);
        return;
    }

    hdma->mem->io[0x55] =
        hdma_read_register(hdma, 0xFF55);

    /*
     * GDMA continues directly into the next block.
     * HBlank DMA waits for the next visible HBlank.
     */
    if (!hdma->hblank_mode)
        hdma_transfer_block(hdma);
}

/*
 * Transfer one byte from the source address
 * into the selected VRAM bank.
 */
static void hdma_transfer_byte(Hdma *hdma)
{
    uint8_t value = mem_read(
        hdma->mem,
        hdma->source);

    uint16_t offset =
        hdma->destination & 0x1FFF;

    hdma->mem->vram[hdma->vram_bank][offset] =
        value;

    hdma->source++;

    hdma->destination = (uint16_t)(0x8000 |
                                   ((hdma->destination + 1) & 0x1FFF));

    hdma->bytes_transferred++;

    if (hdma->bytes_transferred >= HDMA_BLOCK_SIZE)
        hdma_complete_block(hdma);
}

/*
 * Advance the progressive DMA controller.
 *
 * One byte is transferred every two PPU dots.
 * A complete block requires 32 dots.
 */
void hdma_tick(Hdma *hdma, int dots)
{
    if (!hdma || dots <= 0)
        return;

    while (dots > 0 &&
           hdma->active &&
           hdma->block_active)
    {
        dots--;

        hdma->dot_counter++;

        if (hdma->stall_dots > 0)
            hdma->stall_dots--;

        if (hdma->dot_counter >= HDMA_BYTE_DOTS)
        {
            hdma->dot_counter = 0;

            hdma_transfer_byte(hdma);
        }
    }
}

/*
 * Return whether a DMA operation is active,
 * including waiting between HBlank blocks.
 */
int hdma_is_active(const Hdma *hdma)
{
    return hdma && hdma->active;
}

/*
 * The CPU is suspended only while a DMA block
 * is actively being transferred.
 */
int hdma_cpu_stalled(const Hdma *hdma)
{
    return hdma &&
           hdma->active &&
           hdma->block_active;
}
