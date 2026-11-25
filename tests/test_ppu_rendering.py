LCD_W = 160
LCD_H = 144

LCDC = 0x40
SCY = 0x42
SCX = 0x43
LY = 0x44
BGP = 0x47
WY = 0x4A
WX = 0x4B

WHITE = 0xFFFFFFFF
LIGHT_GRAY = 0xFFAAAAAA
DARK_GRAY = 0xFF555555
BLACK = 0xFF000000


def setup_lcd(ppu_lib, rig, lcdc=0x91):
    ppu_lib.rig_set_io(rig, LCDC, lcdc)
    ppu_lib.rig_set_io(rig, BGP, 0xE4)  # IDs 0,1,2,3 map to shades 0,1,2,3.
    ppu_lib.rig_ppu_init(rig)


def render_first_scanline(ppu_lib, rig):
    # 80 dots OAM + 172 dots drawing.
    ppu_lib.rig_ppu_tick(rig, 252)


def test_ppu_initial_framebuffer_is_white(ppu_lib, rig):
    setup_lcd(ppu_lib, rig, lcdc=0x00)

    assert ppu_lib.rig_framebuffer_pixel(rig, 0, 0) == WHITE
    assert ppu_lib.rig_framebuffer_pixel(rig, 159, 143) == WHITE


def test_ppu_enters_oam_then_drawing_then_hblank(ppu_lib, rig):
    setup_lcd(ppu_lib, rig)

    ppu_lib.rig_ppu_tick(rig, 79)
    assert ppu_lib.rig_ppu_mode(rig) == 2

    ppu_lib.rig_ppu_tick(rig, 1)
    assert ppu_lib.rig_ppu_mode(rig) == 3

    ppu_lib.rig_ppu_tick(rig, 172)
    assert ppu_lib.rig_ppu_mode(rig) == 0
    assert ppu_lib.rig_ppu_dot(rig) == 252


def test_dmg_tile_bitplanes_render_expected_shades(ppu_lib, rig):
    setup_lcd(ppu_lib, rig)

    # Tile 0, row 0: IDs from left to right = 3,2,1,0,3,2,1,0.
    ppu_lib.rig_set_vram(rig, 0, 0x0000, 0b10101010)
    ppu_lib.rig_set_vram(rig, 0, 0x0001, 0b01100110)

    render_first_scanline(ppu_lib, rig)

    expected = [
        BLACK, DARK_GRAY, LIGHT_GRAY, WHITE,
        BLACK, DARK_GRAY, LIGHT_GRAY, WHITE,
    ]
    actual = [
        ppu_lib.rig_framebuffer_pixel(rig, x, 0)
        for x in range(8)
    ]
    assert actual == expected


def test_bg_map_selects_the_tile_for_each_eight_pixel_region(ppu_lib, rig):
    setup_lcd(ppu_lib, rig, lcdc=0x99)  # BG map 0x9C00, unsigned tile data.

    # First map cell points to tile 0 (color ID 1); second to tile 1 (ID 2).
    ppu_lib.rig_set_vram(rig, 0, 0x1C00, 0)
    ppu_lib.rig_set_vram(rig, 0, 0x1C01, 1)
    ppu_lib.rig_set_vram(rig, 0, 0, 0xFF)
    ppu_lib.rig_set_vram(rig, 0, 1, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 16, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 17, 0xFF)

    render_first_scanline(ppu_lib, rig)

    assert ppu_lib.rig_framebuffer_pixel(rig, 0, 0) == LIGHT_GRAY
    assert ppu_lib.rig_framebuffer_pixel(rig, 7, 0) == LIGHT_GRAY
    assert ppu_lib.rig_framebuffer_pixel(rig, 8, 0) == DARK_GRAY
    assert ppu_lib.rig_framebuffer_pixel(rig, 15, 0) == DARK_GRAY


def test_scx_scrolls_background_horizontally(ppu_lib, rig):
    setup_lcd(ppu_lib, rig, lcdc=0x91)
    ppu_lib.rig_set_io(rig, SCX, 1)

    # Tile 0 is color 0; tile 1 is color 1.
    ppu_lib.rig_set_vram(rig, 0, 0, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 1, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 16, 0xFF)
    ppu_lib.rig_set_vram(rig, 0, 17, 0x00)

    render_first_scanline(ppu_lib, rig)

    # Screen x=0 reads background x=1, still tile 0.
    assert ppu_lib.rig_framebuffer_pixel(rig, 0, 0) == WHITE


def test_scy_selects_the_background_tile_row(ppu_lib, rig):
    setup_lcd(ppu_lib, rig, lcdc=0x91)
    ppu_lib.rig_set_io(rig, SCY, 1)

    # Row 1 of tile 0 produces color ID 1.
    ppu_lib.rig_set_vram(rig, 0, 2, 0xFF)
    ppu_lib.rig_set_vram(rig, 0, 3, 0x00)

    render_first_scanline(ppu_lib, rig)

    assert ppu_lib.rig_framebuffer_pixel(rig, 0, 0) == LIGHT_GRAY


def test_cgb_bg_palette_rgb555_is_converted_to_argb8888(ppu_lib, rig):
    ppu_lib.rig_set_cgb(rig, 1)
    setup_lcd(ppu_lib, rig, lcdc=0x90)  # CGB draws BG even with LCDC bit 0 clear.

    # Palette 0, color ID 1 = RGB555 red (0x001F), little-endian.
    ppu_lib.rig_set_bg_palette(rig, 2, 0x1F)
    ppu_lib.rig_set_bg_palette(rig, 3, 0x00)

    # Every pixel in tile row has ID 1.
    ppu_lib.rig_set_vram(rig, 0, 0, 0xFF)
    ppu_lib.rig_set_vram(rig, 0, 1, 0x00)

    render_first_scanline(ppu_lib, rig)

    assert ppu_lib.rig_framebuffer_pixel(rig, 0, 0) == 0xFFFF0000


def test_ppu_advances_to_next_scanline(ppu_lib, rig):
    setup_lcd(ppu_lib, rig)

    ppu_lib.rig_ppu_tick(rig, 456)

    assert ppu_lib.rig_ppu_ly(rig) == 1
    assert ppu_lib.rig_ppu_dot(rig) == 0
    assert ppu_lib.rig_ppu_mode(rig) == 2


def test_ppu_enters_vblank_after_144_visible_lines(ppu_lib, rig):
    setup_lcd(ppu_lib, rig)

    ppu_lib.rig_ppu_tick(rig, 456 * 144)

    assert ppu_lib.rig_ppu_ly(rig) == 144
    assert ppu_lib.rig_ppu_mode(rig) == 1
    assert ppu_lib.rig_ppu_frame_ready(rig) == 1
    assert ppu_lib.rig_ppu_frames(rig) == 1


def test_lcd_off_keeps_framebuffer_white_and_produces_frames(ppu_lib, rig):
    setup_lcd(ppu_lib, rig, lcdc=0x00)

    # One complete nominal frame while LCD is off.
    ppu_lib.rig_ppu_tick(rig, 456 * 154)

    assert ppu_lib.rig_ppu_frame_ready(rig) == 1
    assert ppu_lib.rig_ppu_frames(rig) == 1
    assert ppu_lib.rig_framebuffer_pixel(rig, 20, 20) == WHITE
