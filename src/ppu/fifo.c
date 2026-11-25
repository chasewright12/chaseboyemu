
#include "fifo.h"

#include <string.h>

void ppu_fifo_init(PpuFifo *fifo)
{
    if (!fifo)
        return;

    memset(fifo, 0, sizeof(*fifo));
}

void ppu_fifo_clear(PpuFifo *fifo)
{
    if (!fifo)
        return;

    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;
}

int ppu_fifo_push(PpuFifo *fifo, PpuPixel pixel)
{
    if (!fifo || fifo->count >= PPU_FIFO_CAPACITY)
        return 0;

    fifo->pixels[fifo->tail] = pixel;

    fifo->tail =
        (fifo->tail + 1) % PPU_FIFO_CAPACITY;

    fifo->count++;

    return 1;
}

int ppu_fifo_pop(PpuFifo *fifo, PpuPixel *pixel)
{
    if (!fifo || !pixel || fifo->count == 0)
        return 0;

    *pixel = fifo->pixels[fifo->head];

    fifo->head =
        (fifo->head + 1) % PPU_FIFO_CAPACITY;

    fifo->count--;

    return 1;
}

int ppu_fifo_peek(const PpuFifo *fifo, PpuPixel *pixel)
{
    if (!fifo || !pixel || fifo->count == 0)
        return 0;

    *pixel = fifo->pixels[fifo->head];

    return 1;
}

int ppu_fifo_empty(const PpuFifo *fifo)
{
    return !fifo || fifo->count == 0;
}

int ppu_fifo_full(const PpuFifo *fifo)
{
    return fifo && fifo->count >= PPU_FIFO_CAPACITY;
}

unsigned int ppu_fifo_size(const PpuFifo *fifo)
{
    return fifo ? fifo->count : 0;
}

unsigned int ppu_fifo_space(const PpuFifo *fifo)
{
    if (!fifo)
        return 0;

    return PPU_FIFO_CAPACITY - fifo->count;
}
