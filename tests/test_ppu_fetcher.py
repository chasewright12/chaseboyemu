from ppu_test_utils import fetcher_advance, fifo_pop


FETCH_TILE = 0
FETCH_DATA_LOW = 1
FETCH_DATA_HIGH = 2
FETCH_PUSH = 3

LCDC = 0x40


def set_map_entry(lib, rig, offset, tile, attr=0):
    lib.rig_set_vram(rig, 0, offset, tile)
    lib.rig_set_vram(rig, 1, offset, attr)


def test_fetcher_initialization_and_source_alignment(ppu_lib, rig):
    ppu_lib.rig_fetcher_init(rig, 13, 10, 0)

    assert ppu_lib.rig_fetcher_state(rig) == FETCH_TILE
    assert ppu_lib.rig_fetcher_phase(rig) == 0
    assert ppu_lib.rig_fetcher_tile_x(rig) == 8
    assert ppu_lib.rig_fetcher_tile_row(rig) == 0


def test_fetcher_reset_clears_pipeline_state(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x18)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 1)
    ppu_lib.rig_set_vram(rig, 0, 16, 0xAA)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)
    fetcher_advance(ppu_lib, rig, 4)

    assert ppu_lib.rig_fetcher_state(rig) == FETCH_DATA_HIGH
    ppu_lib.rig_fetcher_reset(rig)

    assert ppu_lib.rig_fetcher_state(rig) == FETCH_TILE
    assert ppu_lib.rig_fetcher_phase(rig) == 0
    assert ppu_lib.rig_fetcher_tile_number(rig) == 0
    assert ppu_lib.rig_fetcher_data_low(rig) == 0
    assert ppu_lib.rig_fetcher_data_high(rig) == 0
    assert ppu_lib.rig_fetcher_tile_data_addr(rig) == 0


def test_fetcher_state_transitions_take_two_dots_each(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 1)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_TILE
    assert ppu_lib.rig_fetcher_phase(rig) == 1

    fetcher_advance(ppu_lib, rig, 1)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_DATA_LOW

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_DATA_HIGH

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_PUSH

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_TILE
    assert ppu_lib.rig_fifo_size(rig) == 8


def test_fetcher_reads_bg_map_and_unsigned_tile_data(ppu_lib, rig):
    # BG map at 0x9C00; source tile coordinates x=8, y=16.
    # Map offset = 0x1C00 + (2 * 32) + 1 = 0x1C41.
    ppu_lib.rig_set_io(rig, LCDC, 0x18)
    ppu_lib.rig_set_vram(rig, 0, 0x1C41, 2)
    ppu_lib.rig_set_vram(rig, 0, 2 * 16 + 4, 0xA5)
    ppu_lib.rig_set_vram(rig, 0, 2 * 16 + 5, 0x3C)
    ppu_lib.rig_fetcher_init(rig, 8, 18, 0)

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_tile_map_addr(rig) == 0x1C41
    assert ppu_lib.rig_fetcher_tile_number(rig) == 2
    assert ppu_lib.rig_fetcher_tile_row(rig) == 2
    assert ppu_lib.rig_fetcher_tile_data_addr(rig) == 2 * 16 + 4

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_data_low(rig) == 0xA5

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_data_high(rig) == 0x3C


def test_fetcher_decodes_two_bitplanes_into_eight_pixels(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_set_vram(rig, 0, 0, 0b10101010)
    ppu_lib.rig_set_vram(rig, 0, 1, 0b01100110)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 8)

    pixels = [fifo_pop(ppu_lib, rig) for _ in range(8)]
    assert [p[0] for p in pixels] == [1, 2, 3, 0, 1, 2, 3, 0]
    assert all(p[1:] == (0, 0, 0) for p in pixels)


def test_cgb_attributes_palette_bank_priority_and_horizontal_flip(ppu_lib, rig):
    ppu_lib.rig_set_cgb(rig, 1)
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    # palette 5, bank 1, horizontal flip, BG priority
    attr = 0x80 | 0x20 | 0x08 | 0x05
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_set_vram(rig, 1, 0x1800, attr)
    ppu_lib.rig_set_vram(rig, 1, 0, 0b10000000)
    ppu_lib.rig_set_vram(rig, 1, 1, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 8)
    pixels = [fifo_pop(ppu_lib, rig) for _ in range(8)]

    assert [p[0] for p in pixels] == [0, 0, 0, 0, 0, 0, 0, 1]
    assert all(p[1:] == (5, 1, 1) for p in pixels)


def test_cgb_vertical_flip_selects_reversed_tile_row(ppu_lib, rig):
    ppu_lib.rig_set_cgb(rig, 1)
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 1)
    ppu_lib.rig_set_vram(rig, 1, 0x1800, 0x40)
    ppu_lib.rig_fetcher_init(rig, 0, 2, 0)

    fetcher_advance(ppu_lib, rig, 2)

    assert ppu_lib.rig_fetcher_tile_row(rig) == 5
    assert ppu_lib.rig_fetcher_tile_data_addr(rig) == 16 + 10


def test_signed_tile_number_addressing(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0xFF)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 2)

    assert ppu_lib.rig_fetcher_tile_data_addr(rig) == 0x0FF0


def test_window_uses_window_map_select_bit(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x50)  # window map = 0x9C00, unsigned tiles
    ppu_lib.rig_set_vram(rig, 0, 0x1C00, 7)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 1)

    fetcher_advance(ppu_lib, rig, 2)

    assert ppu_lib.rig_fetcher_tile_map_addr(rig) == 0x1C00
    assert ppu_lib.rig_fetcher_tile_number(rig) == 7


def test_fetcher_push_stalls_when_fifo_has_more_than_eight_pixels(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    for _ in range(9):
        assert ppu_lib.rig_fifo_push_color(rig, 1) == 1

    fetcher_advance(ppu_lib, rig, 6)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_PUSH

    fetcher_advance(ppu_lib, rig, 2)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_PUSH
    assert ppu_lib.rig_fifo_size(rig) == 9

    assert fifo_pop(ppu_lib, rig)[0] == 1
    fetcher_advance(ppu_lib, rig, 1)
    assert ppu_lib.rig_fetcher_state(rig) == FETCH_TILE
    assert ppu_lib.rig_fifo_size(rig) == 16


def test_fetcher_advances_tile_x_after_successful_push(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 8)

    assert ppu_lib.rig_fetcher_tile_x(rig) == 8
