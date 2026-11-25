from ppu_test_utils import fetcher_advance, fifo_pop

LCDC = 0x40

def test_pipeline_outputs_pixels_in_tile_order(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_set_vram(rig, 0, 0x1801, 1)

    # TILE 0: every pixel has color ID 1.
    ppu_lib.rig_set_vram(rig, 0, 0, 0xFF)
    ppu_lib.rig_set_vram(rig, 0, 1, 0x00)

    # TILE 1: every pixel has color ID 2.
    ppu_lib.rig_set_vram(rig, 0, 16, 0x00)
    ppu_lib.rig_set_vram(rig, 0, 17, 0xFF)

    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    output = []
    for _ in range(23):
        ppu_lib.rig_fetcher_tick(rig)
        pixel = fifo_pop(ppu_lib, rig)
        if pixel is not None:
            output.append(pixel[0])

        assert output == [1] * 8 + [2] * 8

def test_pipeline_fifo_count_never_exceeds_capacity(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    for _ in range(200):
        ppu_lib.rig_fetcher_tick(rig)
        assert ppu_lib.rig_fifo_size(rig) <= 16

def test_pipeline_stalls_when_fifo_has_nine_or_more_pixels(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    for _ in range(9):
        assert ppu_lib.rig_fifo_push_color(rig, 2) == 1

    fetcher_advance(ppu_lib, rig, 8)

    assert ppu_lib.rig_fetcher_state(rig) == 3
    assert ppu_lib.rig_fifo_size(rig) == 9

def test_pipeline_can_resume_after_consumer_frees_fifo_space(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    for _ in range(9):
        ppu_lib.rig_fifo_push_color(rig, 2)

    fetcher_advance(ppu_lib, rig, 8)
    assert ppu_lib.rig_fetcher_state(rig) == 3

    assert fifo_pop(ppu_lib, rig)[0] == 2
    ppu_lib.rig_fetcher_tick(rig)

    assert ppu_lib.rig_fetcher_state(rig) == 0
    assert ppu_lib.rig_fifo_size(rig) == 16

def test_pipeline_preserves_cgb_pixel_metadata(ppu_lib, rig):
    ppu_lib.rig_set_cgb(rig, 1)
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)

    # Palette 3, bank 1, priority bit set.
    ppu_lib.rig_set_vram(rig, 1, 0x1800, 0x9B)
    ppu_lib.rig_set_vram(rig, 1, 0, 0xFF)
    ppu_lib.rig_set_vram(rig, 1, 1, 0x00)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 8)
    pixel = fifo_pop(ppu_lib, rig)

    assert pixel == (1, 3, 1, 1)

def test_pipeline_tile_fetcher_moves_to_next_tile(ppu_lib, rig):
    ppu_lib.rig_set_io(rig, LCDC, 0x10)
    ppu_lib.rig_set_vram(rig, 0, 0x1800, 0)
    ppu_lib.rig_set_vram(rig, 0, 0x1801, 1)
    ppu_lib.rig_fetcher_init(rig, 0, 0, 0)

    fetcher_advance(ppu_lib, rig, 8)
    assert ppu_lib.rig_fetcher_tile_x(rig) == 8

    # Drain the first tile so the next tile can be pushed.
    for _ in range(8):
        fifo_pop(ppu_lib, rig)

        fetcher_advance(ppu_lib, rig, 8)
        assert ppu_lib.rig_fifo_size(rig) == 8