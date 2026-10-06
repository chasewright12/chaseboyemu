#include "ppu.h"
#include <string.h>

/* Video registers (indices into mem->io, which starts at 0xFF00) */
#define REG_IF 0x0F
#define REG_LCDC 0x40
#define REG_STAT 0x41
#define REG_SCY 0x42
#define REG_SCX 0x43
#define REG_LY 0x44
#define REG_LYC 0x45
#define REG_BGP 0x47
#define REG_OBP0 0x48
#define REG_OBP1 0x49
#define REG_WY 0x4A
#define REG_WX 0x4B

/* LCDC bits */
#define LCDC_BG_ON 0x01
#define LCDC_OBJ_ON 0x02
#define LCDC_OBJ_16 0x04    /* 8x16 sprites */
#define LCDC_BG_MAP 0x08    /* BG map: 0 = 0x9800, 1 = 0x9C00 */
#define LCDC_TILE_DATA 0x10 /* tile data: 1 = 0x8000 (unsigned), 0 = 0x8800 (signed) */
#define LCDC_WIN_ON 0x20
#define LCDC_WIN_MAP 0x40
#define LCDC_LCD_ON 0x80

/* Line timing, in dots */
#define DOTS_PER_LINE 456
#define DOTS_OAM 80   /* mode 2 */
#define DOTS_DRAW 172 /* mode 3 (the real duration varies; this is the minimum) */
#define LINES_PER_FRAME 154
#define DOTS_PER_FRAME (DOTS_PER_LINE * LINES_PER_FRAME)

/* DMG gray shades: index 0 = white ... 3 = black */
static const uint32_t shades[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};

/* ------------------------------------------------------------------ */
/* STAT and modes                                                      */
/* ------------------------------------------------------------------ */

/* Recomputes STAT (mode and LYC=LY flag) and raises the STAT interrupt
   when the interrupt line goes from 0 to 1. */
static void update_stat(Ppu *p)
{
    Mem *m = p->mem;
    uint8_t stat = m->io[REG_STAT];
    int lyc_eq = m->io[REG_LY] == m->io[REG_LYC];
    int mode = p->lcd_on ? p->mode : 0;

    stat = (uint8_t)((stat & 0x78) | (lyc_eq ? 0x04 : 0) | mode);
    m->io[REG_STAT] = stat;

    int line = p->lcd_on &&
               (((stat & 0x08) && mode == 0) ||
                ((stat & 0x10) && mode == 1) ||
                ((stat & 0x20) && mode == 2) ||
                ((stat & 0x40) && lyc_eq));

    if (line && !p->stat_line)
    {
        m->io[REG_IF] |= 0x02;
    }
    p->stat_line = line;
}

static void set_mode(Ppu *p, int mode)
{
    p->mode = mode;
    update_stat(p);
}

/* ------------------------------------------------------------------ */
/* Scanline rendering                                                  */
/* ------------------------------------------------------------------ */

/* Color (0-3) of pixel 'px' (0-7) in row 'row' (0-7) of the tile whose
   first byte is at VRAM offset 'addr'. */
static int tile_pixel(const Mem *m, int addr, int row, int px)
{
    uint8_t lo = m->vram[0][addr + row * 2];
    uint8_t hi = m->vram[0][addr + row * 2 + 1];
    int bit = 7 - px;
    return ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
}

static void render_scanline(Ppu *p, int ly)
{
    Mem *m = p->mem;
    uint8_t lcdc = m->io[REG_LCDC];
    uint8_t scy = m->io[REG_SCY], scx = m->io[REG_SCX];
    uint8_t bgp = m->io[REG_BGP];
    int wx = m->io[REG_WX];
    uint32_t *line = &p->fb[ly * LCD_W];
    uint8_t bg_id[LCD_W]; /* color of each pixel before the palette (sprite priority needs it) */

    int bg_on = (lcdc & LCDC_BG_ON) != 0;
    int win_on = bg_on && (lcdc & LCDC_WIN_ON) && p->wy_triggered && wx <= 166;
    int win_drawn = 0;

    /* ---- background and window ---- */
    for (int x = 0; x < LCD_W; x++)
    {
        int id = 0;

        if (bg_on)
        {
            int use_win = win_on && x >= wx - 7;
            int map, px, py;

            if (use_win)
            {
                map = (lcdc & LCDC_WIN_MAP) ? 0x1C00 : 0x1800; /* offsets into VRAM */
                px = x - (wx - 7);
                py = p->window_line;
                win_drawn = 1;
            }
            else
            {
                map = (lcdc & LCDC_BG_MAP) ? 0x1C00 : 0x1800;
                px = (x + scx) & 0xFF;
                py = (ly + scy) & 0xFF;
            }

            uint8_t tn = m->vram[0][map + (py / 8) * 32 + px / 8];
            int tile = (lcdc & LCDC_TILE_DATA) ? tn * 16 : 0x1000 + (int8_t)tn * 16;
            id = tile_pixel(m, tile, py & 7, px & 7);
        }

        bg_id[x] = (uint8_t)id;
        line[x] = bg_on ? shades[(bgp >> (id * 2)) & 3] : shades[0];
    }

    if (win_drawn)
    {
        p->window_line++;
    }

    /* ---- sprites ---- */
    if (!(lcdc & LCDC_OBJ_ON))
        return;

    int h = (lcdc & LCDC_OBJ_16) ? 16 : 8;
    int sel[10];
    int n = 0;

    /* The PPU picks the first 10 sprites in OAM that cross this line */
    for (int i = 0; i < 40 && n < 10; i++)
    {
        int y = m->oam[i * 4] - 16;
        if (ly >= y && ly < y + h)
        {
            sel[n++] = i;
        }
    }

    /* DMG priority: lowest X wins; on a tie, the lowest OAM index.
       Sort from highest to lowest priority (stable insertion sort). */
    for (int a = 1; a < n; a++)
    {
        int cur = sel[a];
        int b = a - 1;
        while (b >= 0 && m->oam[sel[b] * 4 + 1] > m->oam[cur * 4 + 1])
        {
            sel[b + 1] = sel[b];
            b--;
        }
        sel[b + 1] = cur;
    }

    uint8_t claimed[LCD_W];
    memset(claimed, 0, sizeof(claimed));

    for (int k = 0; k < n; k++)
    {
        const uint8_t *oam = &m->oam[sel[k] * 4];
        int y = oam[0] - 16;
        int x = oam[1] - 8;
        int tile = oam[2];
        uint8_t flags = oam[3];

        int row = ly - y;
        if (flags & 0x40)
            row = h - 1 - row; /* vertical flip */
        if (h == 16)
            tile &= 0xFE; /* 8x16: the even tile is the top half */

        int addr = tile * 16 + row * 2; /* a row up to 15 runs into the next tile */
        uint8_t lo = m->vram[0][addr];
        uint8_t hi = m->vram[0][addr + 1];
        uint8_t pal = (flags & 0x10) ? m->io[REG_OBP1] : m->io[REG_OBP0];

        for (int px = 0; px < 8; px++)
        {
            int sx = x + px;
            if (sx < 0 || sx >= LCD_W || claimed[sx])
                continue;

            int bit = (flags & 0x20) ? px : 7 - px; /* horizontal flip */
            int id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            if (id == 0)
                continue; /* color 0 is transparent */

            claimed[sx] = 1; /* this sprite beats the lower-priority ones */
            if ((flags & 0x80) && bg_id[sx] != 0)
                continue; /* sprite behind the background */
            line[sx] = shades[(pal >> (id * 2)) & 3];
        }
    }
}

/* ------------------------------------------------------------------ */
/* Line and frame control                                              */
/* ------------------------------------------------------------------ */

static void start_line(Ppu *p, int ly)
{
    Mem *m = p->mem;
    m->io[REG_LY] = (uint8_t)ly;

    if (ly == 0)
    {
        p->window_line = 0;
        p->wy_triggered = 0;
    }

    if (ly < LCD_H)
    {
        if (ly == m->io[REG_WY])
        {
            p->wy_triggered = 1;
        }
        set_mode(p, 2);
    }
    else if (ly == LCD_H)
    {
        m->io[REG_IF] |= 0x01; /* VBlank interrupt */
        p->frame_ready = 1;
        p->frames++;
        set_mode(p, 1);
    }
    else
    {
        update_stat(p);
    }
}

static void lcd_turn_on(Ppu *p)
{
    p->lcd_on = 1;
    p->dot = 0;
    p->off_dots = 0;
    start_line(p, 0);
}

static void lcd_turn_off(Ppu *p)
{
    p->lcd_on = 0;
    p->dot = 0;
    p->mode = 0;
    p->mem->io[REG_LY] = 0;
    for (int i = 0; i < LCD_W * LCD_H; i++)
    {
        p->fb[i] = shades[0];
    }
    update_stat(p);
}

void ppu_init(Ppu *p, Mem *mem)
{
    memset(p, 0, sizeof(*p));
    p->mem = mem;
    for (int i = 0; i < LCD_W * LCD_H; i++)
    {
        p->fb[i] = shades[0];
    }
}

void ppu_tick(Ppu *p, int dots)
{
    Mem *m = p->mem;

    if (!(m->io[REG_LCDC] & LCDC_LCD_ON))
    {
        if (p->lcd_on)
        {
            lcd_turn_off(p);
        }
        /* With the LCD off there is no VBlank, but whoever displays the
           frames must keep receiving them at the normal rate. */
        p->off_dots += dots;
        if (p->off_dots >= DOTS_PER_FRAME)
        {
            p->off_dots -= DOTS_PER_FRAME;
            p->frame_ready = 1;
            p->frames++;
        }
        return;
    }

    if (!p->lcd_on)
    {
        lcd_turn_on(p);
    }

    while (dots-- > 0)
    {
        p->dot++;
        int ly = m->io[REG_LY];

        if (ly < LCD_H)
        {
            if (p->dot == DOTS_OAM)
            {
                set_mode(p, 3);
            }
            else if (p->dot == DOTS_OAM + DOTS_DRAW)
            {
                render_scanline(p, ly);
                set_mode(p, 0);
            }
        }

        if (p->dot == DOTS_PER_LINE)
        {
            p->dot = 0;
            start_line(p, (ly + 1) % LINES_PER_FRAME);
        }
    }

    update_stat(p); /* the CPU may have written LYC or STAT */
}