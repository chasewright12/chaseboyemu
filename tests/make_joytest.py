#!/usr/bin/env python3
"""Builds roms/joytest.gb, a tiny ROM that reads the joypad every frame and draws
the 8 buttons as 8x8 squares along the top of the screen (black = pressed).

Square order, left to right: Right, Left, Up, Down, A, B, Select, Start.

Usage: python3 tests/make_joytest.py [output.gb]
"""
import sys

code = bytearray()
labels = {}
fixups = []   # (position of the offset byte, label)


def emit(*b):
    code.extend(b)


def label(name):
    labels[name] = len(code)


def jr(opcode, name):
    emit(opcode, 0)
    fixups.append((len(code) - 1, name))


emit(0xF3)                       # DI

# wait for VBlank, then turn the LCD off so VRAM can be written
label("wait0")
emit(0xF0, 0x44, 0xFE, 0x90)     # LDH A,(LY) ; CP 144
jr(0x20, "wait0")                # JR NZ
emit(0xAF, 0xE0, 0x40)           # XOR A ; LDH (LCDC),A

# tile 1 = solid color 3
emit(0x21, 0x10, 0x80)           # LD HL,0x8010
emit(0x06, 0x10, 0x3E, 0xFF)     # LD B,16 ; LD A,0xFF
label("fill")
emit(0x22, 0x05)                 # LD (HL+),A ; DEC B
jr(0x20, "fill")                 # JR NZ

emit(0x3E, 0xE4, 0xE0, 0x47)     # LD A,0xE4 ; LDH (BGP),A
emit(0x3E, 0x91, 0xE0, 0x40)     # LD A,0x91 ; LDH (LCDC),A  (LCD and BG on)

label("loop")
label("w1")
emit(0xF0, 0x44, 0xFE, 0x90)     # wait for line 144 (VBlank)
jr(0x20, "w1")

# directions: select with bit 4 = 0, read, invert so 1 = pressed
emit(0x3E, 0x20, 0xE0, 0x00)     # LD A,0x20 ; LDH (P1),A
emit(0xF0, 0x00, 0x2F, 0xE6, 0x0F, 0x47)   # LDH A,(P1) ; CPL ; AND 0F ; LD B,A

# buttons: select with bit 5 = 0
emit(0x3E, 0x10, 0xE0, 0x00)     # LD A,0x10 ; LDH (P1),A
emit(0xF0, 0x00, 0x2F, 0xE6, 0x0F)         # LDH A,(P1) ; CPL ; AND 0F
emit(0xCB, 0x37, 0xB0, 0x4F)     # SWAP A ; OR B ; LD C,A

# draw 8 squares at the top-left of the background map
emit(0x21, 0x00, 0x98, 0x16, 0x08)         # LD HL,0x9800 ; LD D,8
label("bit")
emit(0x79, 0xE6, 0x01, 0x22)     # LD A,C ; AND 1 ; LD (HL+),A
emit(0xCB, 0x39, 0x15)           # SRL C ; DEC D
jr(0x20, "bit")                  # JR NZ

# wait until VBlank is over, then repeat
label("w2")
emit(0xF0, 0x44, 0xFE, 0x90)
jr(0x28, "w2")                   # JR Z
jr(0x18, "loop")                 # JR

for pos, name in fixups:
    off = labels[name] - (pos + 1)
    assert -128 <= off <= 127, name
    code[pos] = off & 0xFF

rom = bytearray(0x8000)
rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])      # NOP ; JP 0x0150
rom[0x134:0x13B] = b"JOYTEST"
rom[0x150:0x150 + len(code)] = code

out = sys.argv[1] if len(sys.argv) > 1 else "roms/joytest.gb"
with open(out, "wb") as f:
    f.write(rom)
print(f"{out}: {len(code)} bytes of code")