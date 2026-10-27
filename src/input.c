#include "input.h"
#include <stdio.h>
#include <string.h>

void input_init(Input *in, Mem *mem)
{
    memset(in, 0, sizeof(*in));
    in->mem = mem;
}

void input_set(Input *in, Button button, InputSource source, int pressed)
{
    int was_down = in->held[button] != 0;

    if (pressed)
    {
        in->held[button] |= (uint8_t)source;
    }
    else
    {
        in->held[button] &= (uint8_t)~source;
    }

    int is_down = in->held[button] != 0;
    if (is_down != was_down)
    {
        joypad_set(in->mem, button, is_down);
    }
}

#ifdef USE_SDL

/* The axis range is -32768..32767: the stick presses at about half and releases at about a quarter */
#define STICK_ON 16000
#define STICK_OFF 8000

static int keyboard_button(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_RIGHT:
        return BTN_RIGHT;
    case SDLK_LEFT:
        return BTN_LEFT;
    case SDLK_UP:
        return BTN_UP;
    case SDLK_DOWN:
        return BTN_DOWN;
    case SDLK_z:
        return BTN_A;
    case SDLK_x:
        return BTN_B;
    case SDLK_BACKSPACE:
        return BTN_SELECT;
    case SDLK_RETURN:
        return BTN_START;
    default:
        return -1;
    }
}

/* SDL gives Xbox and DualShock/DualSense the same layout: SDL's "A" is the bottom
   button (Xbox A, PlayStation Cross) and "B" is the right one (Xbox B, Circle).
   The Game Boy has B on the left and A on the right, so by position the bottom button is B. */
static int pad_button(const Input *in, int button)
{
    switch (button)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_A:
        return in->by_label ? BTN_A : BTN_B;
    case SDL_CONTROLLER_BUTTON_B:
        return in->by_label ? BTN_B : BTN_A;
    case SDL_CONTROLLER_BUTTON_START:
        return BTN_START;
    case SDL_CONTROLLER_BUTTON_BACK:
        return BTN_SELECT;
    default:
        return -1;
    }
}

/* Turns one stick axis into two buttons, with hysteresis so a stick resting
   near the threshold does not flicker */
static void stick_axis(Input *in, Button negative, Button positive, int value)
{
    int neg_held = (in->held[negative] & SRC_STICK) != 0;
    int pos_held = (in->held[positive] & SRC_STICK) != 0;

    input_set(in, negative, SRC_STICK, neg_held ? value < -STICK_OFF : value < -STICK_ON);
    input_set(in, positive, SRC_STICK, pos_held ? value > STICK_OFF : value > STICK_ON);
}

static void pad_add(Input *in, int device_index)
{
    SDL_GameController *pad = SDL_GameControllerOpen(device_index);
    if (!pad)
    {
        fprintf(stderr, "Erro ao abrir o controle: %s\n", SDL_GetError());
        return;
    }
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (in->pads[i] == pad)
        {
            return;
        }
    }
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (!in->pads[i])
        {
            in->pads[i] = pad;
            fprintf(stderr, "Controle conectado: %s\n", SDL_GameControllerName(pad));
            return;
        }
    }
    SDL_GameControllerClose(pad);
}

static void pad_remove(Input *in, SDL_JoystickID instance_id)
{
    SDL_GameController *pad = SDL_GameControllerFromInstanceID(instance_id);

    for (int i = 0; i < MAX_PADS; i++)
    {
        if (in->pads[i] && in->pads[i] == pad)
        {
            SDL_GameControllerClose(pad);
            in->pads[i] = NULL;
        }
    }
    for (int b = 0; b < 8; b++) /* nothing may stay stuck after the pad is gone */
    {
        input_set(in, (Button)b, SRC_PAD, 0);
        input_set(in, (Button)b, SRC_STICK, 0);
    }
    fprintf(stderr, "Controle desconectado\n");
}

int input_handle_event(Input *in, const SDL_Event *e)
{
    int button;

    switch (e->type)
    {
    case SDL_QUIT:
        return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)
        {
            return 1;
        }
        button = keyboard_button(e->key.keysym.sym);
        if (button >= 0 && !e->key.repeat)
        {
            input_set(in, (Button)button, SRC_KEYBOARD, e->type == SDL_KEYDOWN);
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        button = pad_button(in, e->cbutton.button);
        if (button >= 0)
        {
            input_set(in, (Button)button, SRC_PAD, e->type == SDL_CONTROLLERBUTTONDOWN);
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
        {
            stick_axis(in, BTN_LEFT, BTN_RIGHT, e->caxis.value);
        }
        else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
        {
            stick_axis(in, BTN_UP, BTN_DOWN, e->caxis.value);
        }
        break;
    case SDL_CONTROLLERDEVICEADDED:
        pad_add(in, e->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        pad_remove(in, e->cdevice.which);
        break;
    }
    return 0;
}

void input_close(Input *in)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (in->pads[i])
        {
            SDL_GameControllerClose(in->pads[i]);
            in->pads[i] = NULL;
        }
    }
}

#endif