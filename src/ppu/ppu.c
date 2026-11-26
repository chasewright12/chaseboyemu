#include "ppu.h"
#include "../dma/hdma.h"

#include <string.h>

/* Video registers */
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

/* LCDC flags */
#define LCDC_BG_ON 0x01
#define LCDC_OBJ_ON 0x02
#define LCDC_OBJ_16 0x04
#define LCDC_WIN_ON 0x20
#define LCDC_LCD_ON 0x80

/* Timing */
#define DOTS_PER_LINE 456
#define DOTS_OAM 80
#define LINES_PER_FRAME 154
#define DOTS_PER_FRAME (DOTS_PER_LINE * LINES_PER_FRAME)

/* DMG colors */
static const uint32_t shades[4] = {
    0xFFFFFFFF,
    0xFFAAAAAA,
    0xFF555555,
    0xFF000000};

static void update_stat(Ppu *p)
{
    Mem *m = p->mem;

    uint8_t stat = m->io[REG_STAT];
    int lyc_eq = m->io[REG_LY] == m->io[REG_LYC];
    int mode = p->lcd_on ? p->mode : 0;

    stat = (uint8_t)((stat & 0x78) |
                     (lyc_eq ? 0x04 : 0) |
                     mode);

    m->io[REG_STAT] = stat;

    int line = p->lcd_on &&
               (((stat & 0x08) && mode == 0) ||
                ((stat & 0x10) && mode == 1) ||
                ((stat & 0x20) && mode == 2) ||
                ((stat & 0x40) && lyc_eq));

    if (line && !p->stat_line)
        m->io[REG_IF] |= 0x02;

    p->stat_line = line;
}

static void set_mode(Ppu *p, int mode)
{
    int previous_mode = p->mode;
    Mem *m = p->mem;

    p->mode = mode;
    m->ppu_mode = (uint8_t)mode;

    update_stat(p);

    /* Transfer one HDMA block on entry into visible HBlank. */
    if (previous_mode == 3 && mode == 0 &&
        p->lcd_on &&
        m->io[REG_LY] < LCD_H &&
        m->cgb &&
        m->hdma != NULL)
    {
        hdma_hblank(m->hdma);
    }
}

static uint32_t cgb_color(
    const uint8_t *pal,
    int palette,
    int id)
{
    int c =
        pal[palette * 8 + id * 2] |
        (pal[palette * 8 + id * 2 + 1] << 8);

    int r = c & 31;
    int g = (c >> 5) & 31;
    int b = (c >> 10) & 31;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    return 0xFF000000u |
           (uint32_t)(r << 16 | g << 8 | b);
}

static void begin_transfer(Ppu *p, int ly)
{
    Mem *m = p->mem;

    uint8_t scx = m->io[REG_SCX];
    uint8_t scy = m->io[REG_SCY];

    ppu_fifo_clear(&p->bg_fifo);

    ppu_fetcher_init(
        &p->fetcher,
        scx & 0xF8,
        (uint8_t)(ly + scy),
        0);

    p->output_x = 0;
    p->fetch_discard = scx & 7;
    p->window_started = 0;

    memset(p->bg_id, 0, sizeof(p->bg_id));
    memset(p->bg_prio, 0, sizeof(p->bg_prio));
}

/* Start fetching the Window when its screen position is reached. */
static void try_start_window(Ppu *p, int ly)
{
    Mem *m = p->mem;

    uint8_t lcdc = m->io[REG_LCDC];

    int cgb = m->cgb;
    int bg_on = cgb || (lcdc & LCDC_BG_ON);

    int wx = m->io[REG_WX];

    if (p->window_started)
        return;

    if (!bg_on ||
        !(lcdc & LCDC_WIN_ON) ||
        !p->wy_triggered ||
        wx > 166)
    {
        return;
    }

    int window_start = wx - 7;

    if (p->output_x < window_start)
        return;

    /*
     * If WX < 7, part of the Window is already
     * outside the left edge of the screen.
     */
    int source_x = p->output_x - window_start;

    ppu_fifo_clear(&p->bg_fifo);

    ppu_fetcher_init(
        &p->fetcher,
        (uint8_t)source_x,
        (uint8_t)p->window_line,
        1);

    p->fetch_discard = source_x & 7;
    p->window_started = 1;

    (void)ly;
}

static void output_bg_pixel(Ppu *p, int ly)
{
    Mem *m = p->mem;

    uint8_t lcdc = m->io[REG_LCDC];

    int cgb = m->cgb;
    int bg_on = cgb || (lcdc & LCDC_BG_ON);

    if (!bg_on)
    {
        p->bg_id[p->output_x] = 0;
        p->bg_prio[p->output_x] = 0;

        p->fb[ly * LCD_W + p->output_x] = shades[0];

        p->output_x++;
        return;
    }

    try_start_window(p, ly);

    ppu_fetcher_tick(
        &p->fetcher,
        m,
        &p->bg_fifo);

    PpuPixel pixel;

    if (!ppu_fifo_pop(&p->bg_fifo, &pixel))
        return;

    /*
     * SCX and partially hidden Window pixels
     * must be discarded before screen output.
     */
    if (p->fetch_discard > 0)
    {
        p->fetch_discard--;
        return;
    }

    int x = p->output_x;

    p->bg_id[x] = pixel.color_id;
    p->bg_prio[x] = pixel.priority;

    uint32_t color;

    if (cgb)
    {
        color = cgb_color(
            m->bg_pal,
            pixel.palette,
            pixel.color_id);
    }
    else
    {
        uint8_t bgp = m->io[REG_BGP];

        int shade = (bgp >> (pixel.color_id * 2)) & 3;

        color = shades[shade];
    }

    p->fb[ly * LCD_W + x] = color;

    p->output_x++;
}
static void render_sprites(Ppu *p, int ly)
{
    Mem *m = p->mem;

    uint8_t lcdc = m->io[REG_LCDC];

    int cgb = m->cgb;

    if (!(lcdc & LCDC_OBJ_ON))
        return;

    int master_prio = (lcdc & LCDC_BG_ON) != 0;

    int h = (lcdc & LCDC_OBJ_16) ? 16 : 8;

    int sel[10];
    int n = 0;

    /*
     * Select the first ten sprites in OAM
     * that intersect the current scanline.
     */
    for (int i = 0; i < 40 && n < 10; i++)
    {
        int y = m->oam[i * 4] - 16;

        if (ly >= y && ly < y + h)
            sel[n++] = i;
    }

    /*
     * DMG priority:
     * lowest X wins, then lowest OAM index.
     *
     * CGB:
     * OAM order, unless OPRI requests DMG priority.
     */
    if (!cgb || (m->opri & 1))
    {
        for (int a = 1; a < n; a++)
        {
            int cur = sel[a];
            int b = a - 1;

            while (
                b >= 0 &&
                m->oam[sel[b] * 4 + 1] >
                    m->oam[cur * 4 + 1])
            {
                sel[b + 1] = sel[b];
                b--;
            }

            sel[b + 1] = cur;
        }
    }

    uint8_t claimed[LCD_W] = {0};

    for (int k = 0; k < n; k++)
    {
        const uint8_t *oam = &m->oam[sel[k] * 4];

        int y = oam[0] - 16;
        int x = oam[1] - 8;

        int tile = oam[2];

        uint8_t flags = oam[3];

        int row = ly - y;

        if (flags & 0x40)
            row = h - 1 - row;

        if (h == 16)
            tile &= 0xFE;

        int bank = cgb ? (flags >> 3) & 1 : 0;

        int addr = tile * 16 + row * 2;

        uint8_t lo = m->vram[bank][addr];
        uint8_t hi = m->vram[bank][addr + 1];

        uint8_t pal =
            (flags & 0x10)
                ? m->io[REG_OBP1]
                : m->io[REG_OBP0];

        for (int px = 0; px < 8; px++)
        {
            int sx = x + px;

            if (sx < 0 || sx >= LCD_W || claimed[sx])
                continue;

            int bit = (flags & 0x20) ? px : 7 - px;

            int id =
                (((hi >> bit) & 1) << 1) |
                ((lo >> bit) & 1);

            if (id == 0)
                continue;

            claimed[sx] = 1;

            /*
             * BG priority is only enforced when
             * LCDC bit 0 is enabled.
             */
            if (
                master_prio &&
                p->bg_id[sx] != 0 &&
                ((flags & 0x80) || p->bg_prio[sx]))
            {
                continue;
            }

            uint32_t color;

            if (cgb)
            {
                color = cgb_color(
                    m->obj_pal,
                    flags & 7,
                    id);
            }
            else
            {
                int shade = (pal >> (id * 2)) & 3;

                color = shades[shade];
            }

            p->fb[ly * LCD_W + sx] = color;
        }
    }
}
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
            p->wy_triggered = 1;

        p->window_started = 0;

        set_mode(p, 2);
    }
    else if (ly == LCD_H)
    {
        m->io[REG_IF] |= 0x01;

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

    p->mem->ppu_mode = 0;
    p->mem->io[REG_LY] = 0;

    ppu_fifo_clear(&p->bg_fifo);
    ppu_fetcher_reset(&p->fetcher);

    p->output_x = 0;
    p->fetch_discard = 0;
    p->window_started = 0;

    for (int i = 0; i < LCD_W * LCD_H; i++)
        p->fb[i] = shades[0];

    update_stat(p);
}
void ppu_init(Ppu *p, Mem *mem)
{
    memset(p, 0, sizeof(*p));

    p->mem = mem;

    mem->ppu_mode = 0;

    ppu_fifo_init(&p->bg_fifo);
    ppu_fetcher_init(&p->fetcher, 0, 0, 0);

    for (int i = 0; i < LCD_W * LCD_H; i++)
        p->fb[i] = shades[0];
}
void ppu_tick(Ppu *p, int dots)
{
    Mem *m = p->mem;

    if (!(m->io[REG_LCDC] & LCDC_LCD_ON))
    {
        if (p->lcd_on)
            lcd_turn_off(p);

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
        lcd_turn_on(p);

    while (dots-- > 0)
    {
        p->dot++;

        int ly = m->io[REG_LY];

        if (ly < LCD_H)
        {
            if (p->dot == DOTS_OAM)
            {
                begin_transfer(p, ly);

                set_mode(p, 3);
            }
            else if (p->mode == 3)
            {
                output_bg_pixel(p, ly);

                if (p->output_x >= LCD_W)
                {
                    if (p->window_started)
                        p->window_line++;

                    render_sprites(p, ly);

                    set_mode(p, 0);
                }
            }
        }

        if (p->dot == DOTS_PER_LINE)
        {
            p->dot = 0;

            start_line(
                p,
                (ly + 1) % LINES_PER_FRAME);
        }
    }

    /* CPU may have written LYC or STAT. */
    update_stat(p);
}
