
#ifndef PPU_FIFO_H
#define PPU_FIFO_H

#include <stdint.h>
#include <stddef.h>

#define PPU_FIFO_CAPACITY 16

typedef struct
{
    uint8_t color_id;
    uint8_t palette;
    uint8_t priority;
    uint8_t bank;

    uint8_t is_obj;
    uint8_t behind_bg;
    uint8_t oam_index;
} PpuPixel;

typedef struct
{
    PpuPixel pixels[PPU_FIFO_CAPACITY];

    unsigned int head;
    unsigned int tail;
    unsigned int count;

} PpuFifo;

/* Initialize FIFO */
void ppu_fifo_init(PpuFifo *fifo);

/* Clear FIFO */
void ppu_fifo_clear(PpuFifo *fifo);

/* Insert pixel */
int ppu_fifo_push(PpuFifo *fifo, PpuPixel pixel);

/* Remove pixel */
int ppu_fifo_pop(PpuFifo *fifo, PpuPixel *pixel);

/* Read first pixel without removing it */
int ppu_fifo_peek(const PpuFifo *fifo, PpuPixel *pixel);

/* FIFO state */
int ppu_fifo_empty(const PpuFifo *fifo);
int ppu_fifo_full(const PpuFifo *fifo);

unsigned int ppu_fifo_size(const PpuFifo *fifo);
unsigned int ppu_fifo_space(const PpuFifo *fifo);

#endif
