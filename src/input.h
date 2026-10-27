#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include "joypad.h"

#define MAX_PADS 4

#ifdef USE_SDL
#include <SDL.h>
#endif

typedef enum {
    SRC_KEYBOARD = 1,
    SRC_PAD = 2,
    SRC_STICK = 4,
    SRC_SCRIPT = 8
} InputSource;

typedef struct
{
    Mem *mem;
    uint8_t held[8];
    int by_label;

#ifdef USE_SDL
    SDL_GameController *pads[MAX_PADS];
#endif
} Input;

void input_init(Input *in, Mem *mem);
void input_set(Input *in, Button button, InputSource source, int pressed);

#ifdef USE_SDL
int input_handle_event(Input *in, const SDL_Event *e);
void input_close(Input *in);
#endif

#endif