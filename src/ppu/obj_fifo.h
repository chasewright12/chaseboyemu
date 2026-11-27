#ifndef OBJ_FIFO_H
#define OBJ_FIFO_H

#include <stdint.h>

#define OBJ_FIFO_CAPACITY 8

typedef struct
{
    uint8_t color_id;
    uint8_t palette;
    uint8_t priority;
    
    uint8_t oam_index;
    uint8_t sprite_x;
} ObjPixel;

typedef struct {
    ObjPixel pixels[OBJ_FIFO_CAPACITY];

    uint8_t head;
    uint8_t count;
} ObjFifo;

void obj_fifo_init(ObjFifo *fifo);
void obj_fifo_clear(ObjFifo *fifo);

int obj_fifo_empty(const ObjFifo *fifo);
int obj_fifo_full(const ObjFifo *fifo);
int obj_fifo_size(const ObjFifo *fifo);

int obj_fifo_push(ObjFifo *fifo, ObjPixel pixel);
int obj_fifo_pop(ObjFifo *fifo, ObjPixel *pixel);
int obj_fifo_peek(const ObjFifo *fifo, ObjPixel *pixel);

int obj_fifo_merge(
    ObjFifo *fifo,
    const ObjPixel pixels[8],
    int offset,
    int dmg_priority
);

#endif