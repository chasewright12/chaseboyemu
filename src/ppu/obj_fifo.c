#include "obj_fifo.h"
#include <string.h>

void obj_fifo_init(ObjFifo *fifo)
{
    memset(fifo, 0, sizeof(*fifo));
}

void obj_fifo_clear(ObjFifo *fifo)
{
    obj_fifo_init(fifo);
}

int obj_fifo_empty(const ObjFifo *fifo)
{
    return fifo->count == 0;
}

int obj_fifo_full(const ObjFifo *fifo)
{
    return fifo->count >= OBJ_FIFO_CAPACITY;
}

int obj_fifo_size(const ObjFifo *fifo)
{
    return fifo->count;
}

int obj_fifo_push(ObjFifo *fifo, ObjPixel pixel)
{
    if (obj_fifo_full(fifo))
        return 0;

    int index =
        (fifo->head + fifo->count) %
        OBJ_FIFO_CAPACITY;

    fifo->pixels[index] = pixel;

    fifo->count++;

    return 1;
}

int obj_fifo_pop(ObjFifo *fifo, ObjPixel *pixel)
{
    if (obj_fifo_empty(fifo))
        return 0;

    if (pixel)
        *pixel = fifo->pixels[fifo->head];

    fifo->head =
        (fifo->head + 1) % OBJ_FIFO_CAPACITY;

    fifo->count--;
    return 1;
}

int obj_fifo_peek(
    const ObjFifo *fifo,
    ObjPixel *pixel
)
{
    if (obj_fifo_empty(fifo))
        return 0;
    
    if (pixel)
        *pixel = fifo->pixels[fifo->head];

    return 1;
}

/*
*Transparent pixels never replace opaque pixels.
*
*
* DMG priority:
*   Lowest X, then Lowest OAM index.
* CGB priority:
*   Lowest OAM index.
*/
static int obj_pixel_wins(
    const ObjPixel *incoming,
    const ObjPixel *current,
    int dmg_priority
)
{
    if (incoming->color_id == 0)
        return 0;

    if (current->color_id == 0)
        return 1;

    if (dmg_priority)
    {
        if (incoming->sprite_x != current->sprite_x)
        {
            return incoming->sprite_x <
                current->sprite_x;
        }
    }

    return incoming->oam_index <
        current->oam_index;
}

/*
* Merge a fetched sprite row into the FIFO.
*
*
* Negative offsets clip pixels on the left.
* Positive offserts align pixels further ahead.
*/

int obj_fifo_merge(
    ObjFifo *fifo,
    const ObjPixel pixels[8],
    int offset,
    int dmg_priority)
{
    if (offset <= -8 || offset >= OBJ_FIFO_CAPACITY)
        return 0;

    int end = offset + 8;

    if (end > OBJ_FIFO_CAPACITY)
        end = OBJ_FIFO_CAPACITY;

    /*
     * Extend the FIFO with transparent pixels.
     */
    while (fifo->count < end)
    {
        ObjPixel transparent = {0};

        if (!obj_fifo_push(fifo, transparent))
            return 0;
    }

    /*
     * Merge incoming sprite pixels.
     */
    for (int i = 0; i < 8; i++)
    {
        int position = offset + i;

        if (position < 0 ||
            position >= OBJ_FIFO_CAPACITY)
        {
            continue;
        }

        /*
         * Transparent pixels never overwrite
         * existing pixels.
         */
        if (pixels[i].color_id == 0)
            continue;

        int index =
            (fifo->head + position) %
            OBJ_FIFO_CAPACITY;

        ObjPixel *current =
            &fifo->pixels[index];

        if (obj_pixel_wins(
                &pixels[i],
                current,
                dmg_priority))
        {
            *current = pixels[i];
        }
    }

    return 1;
}
