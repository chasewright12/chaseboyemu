<div align="center">

# Chase Boy Emulator

<img src="https://media1.giphy.com/media/v1.Y2lkPTc5MGI3NjExNGEwOTI2c3l3MW9xczRidGk4NWRjM3BrM3AyaHV3cHp2NmI1emRqZyZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/aEUiHbQwoEmc0/giphy.gif" alt="Chase Boy Emu">

**A Game Boy emulator written from scratch in C**, DMG first, with Game Boy Color support in progress.
<br>
Written by **ChaseWright12**.

![Language](https://img.shields.io/badge/C-C11-00599C?style=for-the-badge&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Linux%20%7C%20WSL-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Build](https://img.shields.io/badge/build-make-4EAA25?style=for-the-badge&logo=gnu&logoColor=white)
![Status](https://img.shields.io/badge/status-work%20in%20progress-orange?style=for-the-badge)
![Learning](https://img.shields.io/badge/project-learning-blueviolet?style=for-the-badge)

![Last commit](https://img.shields.io/github/last-commit/chasewright12/gbemu?style=flat-square)
![Code size](https://img.shields.io/github/languages/code-size/chasewright12/gbemu?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square)
![PRs](https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square)

</div>

---

> ⚠️ **Work in progress.** This is a learning project under active development. The CPU, timer and video already pass their test ROMs, but it **cannot play games yet**: there is no joypad input or cartridge bank switching so far. Things will change often (and break). Feedback and suggestions are very welcome.

## 📖 About

**Chase Boy Emulator** is a hands-on study project to understand how the Game Boy and the Game Boy Color work under the hood by rebuilding each piece of their hardware in software. I'm building it to learn C, low-level programming and computer architecture. The end goal is to run real games such as Tetris, with video, controls and, last of all, sound.

Because the main goal is learning, the code favors **clarity over performance**: simple structures, no global variables, and each component's state kept in a `struct` passed by pointer.

## 🖼️ Screenshot

The emulator rendering [dmg-acid2](https://github.com/mattcurrie/dmg-acid2), a test ROM that exercises the background, window and sprites. The output matches the official reference image **pixel for pixel**.

<div align="center">

<img src="docs/dmg-acid2.png" width="320" alt="dmg-acid2 rendered by the emulator">

</div>

## ✨ Current status

| Component | Status | Notes |
|---|:---:|---|
| ROM loading and header parsing | ✅ | Title, Game Boy / Color mode, cartridge type and ROM size |
| Memory bus | ✅ | ROM, VRAM, RAM, Echo RAM, OAM, I/O, HRAM, `IE` and OAM DMA |
| CPU (Sharp LR35902) | ✅ | Full instruction set, `0xCB` prefix, interrupts, `HALT` and delayed `EI` |
| Timer | ✅ | `DIV`, `TIMA`, `TMA` and `TAC`, including the falling-edge behavior |
| Blargg `cpu_instrs` | ✅ | The 11 individual ROMs pass, in both DMG and Color mode |
| PPU (video, DMG) | ✅ | Background, window, sprites, STAT/VBlank interrupts. Passes `dmg-acid2` |
| Screenshots and SDL2 window | ✅ | PNG output works anywhere; the window is optional (`make SDL=1`) |
| Game Boy Color | 🚧 | Banked VRAM/WRAM, speed switch and CGB start-up state. Color palettes and tile attributes are still missing |
| Joypad | ⬜ | |
| MBC1 / MBC3 / MBC5 | ⬜ | ROM/RAM bank switching (also needed for the full `cpu_instrs.gb`) |
| APU (audio) | ⬜ | The 4 sound channels |
| Saves (battery-backed RAM) | ⬜ | |

## 🛠️ Requirements

- A Linux system (or **WSL** on Windows)
- `gcc` (or `clang`) and `make`
- *Optional:* `libsdl2-dev`, to open a window

On Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential git
sudo apt install libsdl2-dev   # optional, for the window
```

## 🚀 Building and running

```bash
# 1. clone the repository
git clone https://github.com/chasewright12/gbemu.git
cd gbemu

# 2. build
make            # no window (headless)
make SDL=1      # with an SDL2 window

# 3. run with a ROM
./gbemu path/to/rom.gb
```

To remove the generated executable:

```bash
make clean
```

### Command-line options

| Option | What it does |
|---|---|
| `--frames N` | Runs exactly N frames and stops |
| `--png FILE` | Saves the last frame as a PNG (enlarged 4x), with no external library |
| `--headless` | Does not open a window, even in a build made with `SDL=1` |
| `--doctor` | Prints one log line per instruction in the [Gameboy Doctor](https://github.com/robert/gameboy-doctor) format |

Without `SDL=1`, the emulator always runs headless. With it, a window opens at 4x scale, paced at the real Game Boy speed (about 59.7 frames per second). Close it with `Esc`.

> If the window does not open on WSL, check that your WSL has graphical support (WSLg). The `--png` option works either way.

## 🧪 Testing

ROMs are **not** included in this repository. The tests below use public test ROMs.

### CPU and timer: Blargg's `cpu_instrs`

```bash
git clone https://github.com/retrio/gb-test-roms.git roms/gb-test-roms

for rom in roms/gb-test-roms/cpu_instrs/individual/*.gb; do
  echo "== $(basename "$rom")"
  ./gbemu "$rom" 2>&1 >/dev/null | grep -E "Passed|Failed"
done
```

All 11 ROMs should print `Passed`. A single ROM looks like this (the messages are still in Portuguese):

```
Titulo:
Modo: Game Boy Color (compativel com DMG)
Tipo do cartucho: 0x01
Codigo do tamanho da ROM: 0x00
Tamanho do arquivo: 32768 bytes
01-special


Passed
Ciclos executados: 9810784 | Quadros: 139
```

When a test fails, the ROM prints which opcodes are wrong. The complete `cpu_instrs.gb` (one 64 KB ROM) needs MBC1, which is not implemented yet.

### Video: dmg-acid2

```bash
curl -L -o roms/dmg-acid2.gb https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb
./gbemu roms/dmg-acid2.gb --frames 60 --png acid.png
```

Open `acid.png` and compare it with the [reference image](https://github.com/mattcurrie/dmg-acid2/blob/master/img/reference-dmg.png). They should be identical.

## 🗂️ Project structure

```
gbemu/
├── Makefile
├── README.md
├── docs/
│   └── dmg-acid2.png   # screenshot used in this README
└── src/
    ├── main.c        # entry point, command-line options and the main loop
    ├── cart.c/h      # ROM loading and header parsing
    ├── mem.c/h       # memory bus (address map, banking, DMA, serial port)
    ├── cpu.c/h       # registers, flags, instructions and interrupts
    ├── timer.c/h     # DIV, TIMA, TMA and TAC
    ├── ppu.c/h       # video: timing, background, window and sprites
    └── png.c/h       # PNG writer (no external library)
```

## 🧠 How it works

Every instruction runs in the same loop: the CPU executes it and reports how many cycles it took, and the other components advance by that same amount of time. This is what keeps the timer and the video in sync with the CPU.

### Memory bus

The Game Boy CPU sees a single 64 KB address space, but each range of addresses is a different piece of hardware. The memory bus (`mem_read` / `mem_write`) translates each address to the right region:

| Range | Region |
|---|---|
| `0x0000`–`0x7FFF` | Cartridge ROM |
| `0x8000`–`0x9FFF` | VRAM (two banks on Color) |
| `0xA000`–`0xBFFF` | Cartridge RAM |
| `0xC000`–`0xDFFF` | WRAM (eight banks on Color) |
| `0xE000`–`0xFDFF` | Echo RAM (mirror of WRAM) |
| `0xFE00`–`0xFE9F` | OAM (sprites) |
| `0xFF00`–`0xFF7F` | I/O registers |
| `0xFF80`–`0xFFFE` | HRAM |
| `0xFFFF` | Interrupt enable register |

### CPU

The CPU uses the registers `A`, `F`, `B`, `C`, `D`, `E`, `H`, `L`, plus `SP` and `PC`. The `F` register holds the flags:

| Bit | Flag | Meaning |
|:---:|:---:|---|
| 7 | `Z` | Zero |
| 6 | `N` | Subtraction |
| 5 | `H` | Half-carry |
| 4 | `C` | Carry |

`cpu_step` first services any pending interrupt, then fetches an opcode, decodes it, executes it and returns the cycles spent. Most opcodes are decoded from their bit patterns instead of one `case` each, which is why the whole instruction set fits in about 600 lines.

### Timer

`DIV` is the high byte of a 16-bit counter that increments every cycle. `TIMA` increments when a selected bit of that counter falls from 1 to 0, and the `TAC` register chooses which bit (four frequencies, from 4096 Hz to 262144 Hz). When `TIMA` overflows it reloads from `TMA` and requests an interrupt. Writing to `DIV` or `TAC` can also make `TIMA` tick, as on the real hardware.

### PPU

Each line takes 456 dots and goes through three modes (OAM scan, drawing and HBlank), and a frame is 154 lines (144 visible plus 10 of VBlank), or 70224 dots. At the end of the drawing mode each line is rendered in three layers: background, window and sprites (up to 10 per line, with the DMG priority rules). The PPU also raises the VBlank interrupt and the STAT interrupts (HBlank, VBlank, OAM and `LYC=LY`).

Known simplifications: mode 3 always lasts 172 dots, and the CPU can access VRAM and OAM at any time.

## 🗺️ Roadmap

- [x] Load the ROM and read its header
- [x] Memory bus
- [x] CPU structure and first opcodes
- [x] Implement all opcodes (including the `0xCB` prefix)
- [x] Pass Blargg's `cpu_instrs` tests (the 11 individual ROMs)
- [x] Timer and interrupts
- [x] PPU for the DMG: background, window and sprites (`dmg-acid2`)
- [x] PNG screenshots and an optional SDL2 window
- [x] Game Boy Color groundwork: VRAM/WRAM banks and speed switch
- [ ] Joypad via keyboard
- [ ] **Run Tetris** 🎉
- [ ] MBC1, then MBC3 and MBC5
- [ ] Accurate cycle counting (`instr_timing`)
- [ ] Game Boy Color video: palettes, tile attributes and HDMA (`cgb-acid2`)
- [ ] Audio (APU)
- [ ] Save files

## 🤝 Contributing

Since this is a learning project, I'm especially happy to receive:

- Explanations of things I got wrong or implemented in a roundabout way
- Pointers to good resources about the Game Boy hardware
- Bug reports and suggestions

Feel free to open an issue or a pull request. Please keep in mind that I'm doing most of the implementation myself on purpose, so I may prefer to discuss a fix before merging a large change.

## ⚖️ Legal notice

This project is an emulator and **does not include any ROMs or Nintendo code**. Emulating is legal, but distributing commercial ROMs is not. Use only test ROMs, homebrew, or dumps of cartridges you own.

*Game Boy is a trademark of Nintendo. This project is not affiliated with or endorsed by Nintendo.*

## 📚 References

- [Pan Docs](https://gbdev.io/pandocs/): the main hardware reference
- [Opcode table](https://gbdev.io/gb-opcodes/optables/)
- [gb-test-roms](https://github.com/retrio/gb-test-roms): Blargg's test ROMs
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2): PPU test ROM
- [Gameboy Doctor](https://github.com/robert/gameboy-doctor): CPU log comparison tool
- [SameBoy](https://github.com/LIJI32/SameBoy) and [Gambatte](https://github.com/sinamas/gambatte): reference emulators

## 📄 License

Distributed under the MIT License. See the `LICENSE` file for more details.

---

<div align="center">

<a href="https://github.com/chasewright12">
  <img src="https://images.weserv.nl/?url=github.com/chasewright12.png&w=200&h=200&fit=cover&mask=circle" width="150" alt="Lucas" />
</a>

### Lucas

Brazilian student passionate about programming and computer science.
I build software on the side and I'm learning how computers work by creating this emulator from scratch.

[![GitHub](https://img.shields.io/badge/GitHub-chasewright12-181717?style=flat-square&logo=github)](https://github.com/chasewright12)
[![LinkedIn](https://img.shields.io/badge/LinkedIn-lucasmarquesdev-0A66C2?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/lucasmarquesdev/)


**A Brazilian open-source project**. 
If you like the project or learned something from it, consider giving it a star!

</div>