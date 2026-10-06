#ifndef PNG_H
#define PNG_H

#include <stdint.h>

/* Saves a 0xAARRGGBB image (width w, height h) as a PNG, enlarged 'scale' times.
   Returns 1 on success, 0 on error. Needs no external library. */
int png_save(const char *path, const uint32_t *pixels, int w, int h, int scale);

#endif