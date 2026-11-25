
import ctypes
import subprocess
import tempfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
FIFO_SOURCE = ROOT / "src" / "ppu" / "fifo.c"

FIFO_CAPACITY = 16

class PpuPixel(ctypes.Structure):
    _fields_ = [
        ("color_id", ctypes.c_uint8),
        ("palette", ctypes.c_uint8),
        ("priority", ctypes.c_uint8),
        ("bank", ctypes.c_uint8),
        ("is_obj", ctypes.c_uint8),
        ("behind_bg", ctypes.c_uint8),
        ("oam_index", ctypes.c_uint8),
    ]


class PpuFifo(ctypes.Structure):
    _fields_ = [
        ("pixels", PpuPixel * FIFO_CAPACITY),
        ("head", ctypes.c_uint),
        ("tail", ctypes.c_uint),
        ("count", ctypes.c_uint),
    ]

@pytest.fixture(scope="session")
def fifo_lib():

    if not FIFO_SOURCE.exists():
        pytest.fail(f"FIFO source not found: {FIFO_SOURCE}")

    with tempfile.TemporaryDirectory() as temp_dir:

        library_path = Path(temp_dir) / "libppu_fifo.so"

        command = [
            "gcc",
            "-shared",
            "-fPIC",
            "-Wall",
            "-Wextra",
            "-std=c11",
            "-g",
            "-O2",
            str(FIFO_SOURCE),
            "-o",
            str(library_path),
        ]

        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            pytest.fail(
                "FIFO compilation failed:\n"
                + result.stderr
            )

        lib = ctypes.CDLL(str(library_path))

        lib.ppu_fifo_init.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_init.restype = None

        lib.ppu_fifo_clear.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_clear.restype = None

        lib.ppu_fifo_push.argtypes = [
            ctypes.POINTER(PpuFifo),
            PpuPixel,
        ]
        lib.ppu_fifo_push.restype = ctypes.c_int

        lib.ppu_fifo_pop.argtypes = [
            ctypes.POINTER(PpuFifo),
            ctypes.POINTER(PpuPixel),
        ]
        lib.ppu_fifo_pop.restype = ctypes.c_int

        lib.ppu_fifo_peek.argtypes = [
            ctypes.POINTER(PpuFifo),
            ctypes.POINTER(PpuPixel),
        ]
        lib.ppu_fifo_peek.restype = ctypes.c_int

        lib.ppu_fifo_empty.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_empty.restype = ctypes.c_int

        lib.ppu_fifo_full.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_full.restype = ctypes.c_int

        lib.ppu_fifo_size.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_size.restype = ctypes.c_uint

        lib.ppu_fifo_space.argtypes = [
            ctypes.POINTER(PpuFifo)
        ]
        lib.ppu_fifo_space.restype = ctypes.c_uint

        yield lib


@pytest.fixture
def fifo(fifo_lib):

    instance = PpuFifo()

    fifo_lib.ppu_fifo_init(ctypes.byref(instance))

    return instance

def make_pixel(
    color_id=0,
    palette=0,
    priority=0,
    bank=0,
    is_obj=0,
    behind_bg=0,
    oam_index=0,
):
    return PpuPixel(
        color_id,
        palette,
        priority,
        bank,
        is_obj,
        behind_bg,
        oam_index,
    )


def assert_pixel_equal(actual, expected):

    assert actual.color_id == expected.color_id
    assert actual.palette == expected.palette
    assert actual.priority == expected.priority
    assert actual.bank == expected.bank
    assert actual.is_obj == expected.is_obj
    assert actual.behind_bg == expected.behind_bg
    assert actual.oam_index == expected.oam_index

def test_fifo_initialization(fifo_lib, fifo):

    assert fifo.head == 0
    assert fifo.tail == 0
    assert fifo.count == 0

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 1

    assert fifo_lib.ppu_fifo_full(
        ctypes.byref(fifo)
    ) == 0

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 0

    assert fifo_lib.ppu_fifo_space(
        ctypes.byref(fifo)
    ) == FIFO_CAPACITY

def test_fifo_push(fifo_lib, fifo):

    pixel = make_pixel(
        color_id=3,
        palette=2,
        priority=1,
        bank=1,
    )

    result = fifo_lib.ppu_fifo_push(
        ctypes.byref(fifo),
        pixel,
    )

    assert result == 1

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 1

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 0


def test_fifo_push_full_capacity(fifo_lib, fifo):

    for i in range(FIFO_CAPACITY):

        pixel = make_pixel(color_id=i)

        result = fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            pixel,
        )

        assert result == 1

    assert fifo_lib.ppu_fifo_full(
        ctypes.byref(fifo)
    ) == 1

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == FIFO_CAPACITY

    assert fifo_lib.ppu_fifo_space(
        ctypes.byref(fifo)
    ) == 0


def test_fifo_overflow(fifo_lib, fifo):

    for i in range(FIFO_CAPACITY):

        assert fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        ) == 1

    extra_pixel = make_pixel(color_id=99)

    result = fifo_lib.ppu_fifo_push(
        ctypes.byref(fifo),
        extra_pixel,
    )

    assert result == 0

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == FIFO_CAPACITY

def test_fifo_pop(fifo_lib, fifo):

    expected = make_pixel(
        color_id=2,
        palette=1,
        priority=1,
        bank=0,
    )

    fifo_lib.ppu_fifo_push(
        ctypes.byref(fifo),
        expected,
    )

    output = PpuPixel()

    result = fifo_lib.ppu_fifo_pop(
        ctypes.byref(fifo),
        ctypes.byref(output),
    )

    assert result == 1

    assert_pixel_equal(output, expected)

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 1


def test_fifo_underflow(fifo_lib, fifo):

    output = PpuPixel()

    result = fifo_lib.ppu_fifo_pop(
        ctypes.byref(fifo),
        ctypes.byref(output),
    )

    assert result == 0

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 0

def test_fifo_preserves_order(fifo_lib, fifo):

    for i in range(FIFO_CAPACITY):

        fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        )

    for i in range(FIFO_CAPACITY):

        output = PpuPixel()

        result = fifo_lib.ppu_fifo_pop(
            ctypes.byref(fifo),
            ctypes.byref(output),
        )

        assert result == 1
        assert output.color_id == i

def test_fifo_peek(fifo_lib, fifo):

    expected = make_pixel(
        color_id=3,
        palette=2,
        bank=1,
        is_obj=1,
        behind_bg=1,
        oam_index=7,
    )

    fifo_lib.ppu_fifo_push(
        ctypes.byref(fifo),
        expected,
    )

    output = PpuPixel()

    result = fifo_lib.ppu_fifo_peek(
        ctypes.byref(fifo),
        ctypes.byref(output),
    )

    assert result == 1

    assert_pixel_equal(output, expected)

    # Peek must not remove the pixel.
    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 1


def test_fifo_peek_empty(fifo_lib, fifo):

    output = PpuPixel()

    result = fifo_lib.ppu_fifo_peek(
        ctypes.byref(fifo),
        ctypes.byref(output),
    )

    assert result == 0

def test_fifo_clear(fifo_lib, fifo):

    for i in range(8):

        fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        )

    fifo_lib.ppu_fifo_clear(
        ctypes.byref(fifo)
    )

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 1

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 0

    assert fifo_lib.ppu_fifo_space(
        ctypes.byref(fifo)
    ) == FIFO_CAPACITY

def test_fifo_wraparound(fifo_lib, fifo):

    # Fill the FIFO.
    for i in range(FIFO_CAPACITY):

        assert fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        ) == 1

    # Remove the first 8 pixels.
    for i in range(8):

        output = PpuPixel()

        assert fifo_lib.ppu_fifo_pop(
            ctypes.byref(fifo),
            ctypes.byref(output),
        ) == 1

        assert output.color_id == i

    # Insert 8 new pixels.
    # This should exercise circular buffer wrapping.
    for i in range(8):

        assert fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=16 + i),
        ) == 1

    assert fifo_lib.ppu_fifo_full(
        ctypes.byref(fifo)
    ) == 1

    # Verify the remaining FIFO order.
    expected_values = list(range(8, 16)) + list(range(16, 24))

    for expected in expected_values:

        output = PpuPixel()

        assert fifo_lib.ppu_fifo_pop(
            ctypes.byref(fifo),
            ctypes.byref(output),
        ) == 1

        assert output.color_id == expected

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 1

def test_fifo_preserves_pixel_metadata(fifo_lib, fifo):

    expected = make_pixel(
        color_id=3,
        palette=5,
        priority=1,
        bank=1,
        is_obj=1,
        behind_bg=1,
        oam_index=39,
    )

    fifo_lib.ppu_fifo_push(
        ctypes.byref(fifo),
        expected,
    )

    output = PpuPixel()

    fifo_lib.ppu_fifo_pop(
        ctypes.byref(fifo),
        ctypes.byref(output),
    )

    assert_pixel_equal(output, expected)


def test_fifo_mixed_operations(fifo_lib, fifo):

    # Push 5 pixels.
    for i in range(5):

        assert fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        ) == 1

    # Peek at the first pixel.
    output = PpuPixel()

    assert fifo_lib.ppu_fifo_peek(
        ctypes.byref(fifo),
        ctypes.byref(output),
    ) == 1

    assert output.color_id == 0

    # Pop 2 pixels.
    for i in range(2):

        assert fifo_lib.ppu_fifo_pop(
            ctypes.byref(fifo),
            ctypes.byref(output),
        ) == 1

        assert output.color_id == i

    # Push 3 more pixels.
    for i in range(5, 8):

        assert fifo_lib.ppu_fifo_push(
            ctypes.byref(fifo),
            make_pixel(color_id=i),
        ) == 1

    assert fifo_lib.ppu_fifo_size(
        ctypes.byref(fifo)
    ) == 6

    # Verify remaining order.
    for expected in range(2, 8):

        assert fifo_lib.ppu_fifo_pop(
            ctypes.byref(fifo),
            ctypes.byref(output),
        ) == 1

        assert output.color_id == expected

    assert fifo_lib.ppu_fifo_empty(
        ctypes.byref(fifo)
    ) == 1
