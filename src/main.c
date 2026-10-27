#include "cart.h"
#include "mem.h"
#include "cpu.h"
#include "timer.h"
#include "ppu.h"
#include "joypad.h"
#include "input.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif

#define MAX_CYCLES 1000000000ULL /* safety limit in headless mode (~4 minutes of Game Boy time) */
#define WINDOW_SCALE 4

#define MAX_PRESSES 16

/* A scripted button press: held from frame 'start' up to (not including) frame 'end' */
typedef struct
{
    Button button;
    uint64_t start, end;
    int down;
} Press;

typedef struct
{
    Cpu cpu;
    Mem mem;
    Ppu ppu;
    Press presses[MAX_PRESSES];
    int npresses;
} Emu;

static void apply_presses(Emu *emu)
{
    for (int i = 0; i < emu->npresses; i++)
    {
        Press *p = &emu->presses[i];
        int want = emu->ppu.frames >= p->start && emu->ppu.frames < p->end;
        if (want != p->down)
        {
            joypad_set(&emu->mem, p->button, want);
            p->down = want;
        }
    }
}

/* Parses "button:start:end", for example "start:30:40" */
static int parse_press(const char *arg, Press *p)
{
    static const char *names[] = {"right", "left", "up", "down", "a", "b", "select", "start"};
    char name[16];
    unsigned long long start, end;

    if (sscanf(arg, "%15[^:]:%llu:%llu", name, &start, &end) != 3)
    {
        return 0;
    }
    for (int i = 0; i < 8; i++)
    {
        if (strcmp(name, names[i]) == 0)
        {
            p->button = (Button)i;
            p->start = start;
            p->end = end;
            p->down = 0;
            return 1;
        }
    }
    return 0;
}

/* Runs one instruction and advances every component by the same amount of time.
   Returns the cycles spent, or -1 if the CPU stopped (invalid opcode). */
static int emu_step(Emu *emu)
{
    apply_presses(emu);
    int cycles = cpu_step(&emu->cpu);
    if (cycles < 0)
    {
        return -1;
    }
    timer_tick(&emu->mem, cycles);

    /* In CGB double speed the CPU runs twice as fast as the PPU */
    ppu_tick(&emu->ppu, emu->mem.double_speed ? cycles / 2 : cycles);
    return cycles;
}

#ifdef USE_SDL

static void run_window(Emu *emu, const char *title, uint64_t frame_limit)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fprintf(stderr, "Erro ao iniciar o SDL: %s\n", SDL_GetError());
        return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);

    Input input;
    input_init(&input, &emu->mem);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Window *win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       LCD_W * WINDOW_SCALE, LCD_H * WINDOW_SCALE, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
    SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                               SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H)
                           : NULL;
    if (!tex)
    {
        fprintf(stderr, "Erro ao criar a janela: %s\n", SDL_GetError());

        input_close(&input);

        if (ren)
            SDL_DestroyRenderer(ren);

        if (win)
            SDL_DestroyWindow(win);

        SDL_Quit();
        return;
    }

    const double frame_seconds = 70224.0 / 4194304.0;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 next = SDL_GetPerformanceCounter();
    int running = 1;

    while (running)
    {
        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            if (input_handle_event(&input, &e))
            {
                running = 0;
            }
        }

        while (!emu->ppu.frame_ready)
        {
            if (emu_step(emu) < 0)
            {
                running = 0;
                break;
            }
        }

        emu->ppu.frame_ready = 0;

        SDL_UpdateTexture(
            tex,
            NULL,
            emu->ppu.fb,
            LCD_W * (int)sizeof(uint32_t));

        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        if (frame_limit && emu->ppu.frames >= frame_limit)
        {
            running = 0;
        }

        next += (Uint64)(frame_seconds * (double)freq);
        Uint64 now = SDL_GetPerformanceCounter();

        if (next > now)
        {
            SDL_Delay((Uint32)((next - now) * 1000 / freq));
        }
        else
        {
            next = now;
        }
    }

    input_close(&input);

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
#endif

static void usage(const char *prog)
{
    fprintf(stderr,
            "Uso: %s <rom.gb> [opcoes]\n"
            "  --frames N     para depois de N quadros\n"
            "  --png ARQUIVO  salva o ultimo quadro em PNG (ampliado %dx)\n"
            "  --headless     nao abre janela (roda ate Passed/Failed das ROMs de teste)\n"
            "  --press B:I:F  segura o botao B do quadro I ate o quadro F\n"
            "                 (right left up down a b select start), pode repetir\n"
            "  --doctor       log de cada instrucao no formato do Gameboy Doctor\n",
            prog, WINDOW_SCALE);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    const char *png_path = NULL;
    const char *press_args[MAX_PRESSES];
    int npress_args = 0;
    uint64_t frame_limit = 0;
    int doctor = 0;
#ifdef USE_SDL
    int headless = 0;
#else
    int headless = 1;
#endif

    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--doctor") == 0)
        {
            doctor = 1;
        }
        else if (strcmp(argv[i], "--headless") == 0)
        {
            headless = 1;
        }
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
            frame_limit = strtoull(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
        {
            png_path = argv[++i];
        }
        else if (strcmp(argv[i], "--press") == 0 && i + 1 < argc && npress_args < MAX_PRESSES)
        {
            press_args[npress_args++] = argv[++i];
        }
        else
        {
            usage(argv[0]);
            return 1;
        }
    }

    Cart cart;
    if (!cart_load(&cart, argv[1]))
    {
        return 1;
    }
    if (!doctor)
    {
        cart_print_header(&cart);
    }

    Emu emu;
    mem_init(&emu.mem, &cart);
    emu.mem.ly_stub = doctor; /* Gameboy Doctor requires LY to always read 0x90 */
    cpu_init(&emu.cpu, &emu.mem);
    ppu_init(&emu.ppu, &emu.mem);

    emu.npresses = 0;
    for (int i = 0; i < npress_args; i++)
    {
        if (!parse_press(press_args[i], &emu.presses[emu.npresses++]))
        {
            fprintf(stderr, "Argumento invalido em --press: %s\n", press_args[i]);
            cart_free(&cart);
            return 1;
        }
    }

    if (headless)
    {
        /* No window: with --frames, run exactly N frames; without it, run until
           the test finishes (Passed/Failed). Both are capped by the cycle limit. */
        while (emu.cpu.cycles < MAX_CYCLES &&
               (frame_limit ? emu.ppu.frames < frame_limit : !emu.mem.test_done))
        {
            if (doctor)
            {
                cpu_log_doctor(&emu.cpu);
            }
            if (emu_step(&emu) < 0)
            {
                break;
            }
        }
    }
#ifdef USE_SDL
    else
    {
        run_window(&emu, "gbemu", frame_limit);
    }
#endif

    if (!doctor)
    {
        fprintf(stderr, "\nCiclos executados: %llu | Quadros: %llu\n",
                (unsigned long long)emu.cpu.cycles, (unsigned long long)emu.ppu.frames);
    }

    if (png_path)
    {
        if (png_save(png_path, emu.ppu.fb, LCD_W, LCD_H, WINDOW_SCALE))
        {
            fprintf(stderr, "Quadro salvo em %s\n", png_path);
        }
        else
        {
            fprintf(stderr, "Erro ao salvar %s\n", png_path);
        }
    }

    cart_free(&cart);
    return 0;
}