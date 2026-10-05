<div align="center">

# Chase Boy Emulator

**A Game Boy (DMG) emulator written from scratch in C**

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

> ⚠️ **Work in progress.** This is a learning project under active development. It **cannot run games yet**, parts of the code are incomplete, and things will change often (and break). Feedback and suggestions are very welcome.

## 📖 About

**Chase Boy Emulator** is a hands-on study project to understand how the Game Boy Color works under the hood by rebuilding each piece of its hardware in software. I'm building it to learn C, low-level programming and computer architecture. The end goal is to run real games such as Tetris, with video, controls and, last of all, sound.

Because the main goal is learning, the code favors **clarity over performance**: simple structures, no global variables, and each component's state kept in a `struct` passed by pointer.

## ✨ Current status

| Component | Status | Notes |
|---|:---:|---|
| ROM loading and header parsing | ✅ | Title, cartridge type and ROM size |
| Memory bus | ✅ | ROM, VRAM, RAM, Echo RAM, OAM, I/O, HRAM and `IE` |
| CPU (Sharp LR35902) | ✅ | Fetch/decode/execute loop and a few opcodes |
| Timer and interrupts | 🚧 | |
| Blargg test ROMs | ⬜ | `cpu_instrs` |
| PPU (video) | ⬜ | Background, window and sprites |
| Joypad | ⬜ | |
| MBC1 / MBC3 / MBC5 | ⬜ | ROM/RAM bank switching |
| APU (audio) | ⬜ | The 4 sound channels |
| Saves (battery-backed RAM) | ⬜ | |

**Opcodes implemented so far:** `NOP`, `LD r, d8`, `LD BC/DE/HL/SP, d16`, `XOR A`, `JP a16` and `DI`.

## 🛠️ Requirements

- A Linux system (or **WSL** on Windows)
- `gcc` (or `clang`) and `make`

On Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential git
```

## 🚀 Building and running

```bash
# 1. clone the repository
git clone https://github.com/SEU_USUARIO/gbemu.git
cd gbemu

# 2. build
make

# 3. run with a ROM
./gbemu path/to/rom.gb
```

To remove the generated executable:

```bash
make clean
```

### Getting test ROMs

ROMs are **not** included in this repository. To test the CPU, download Blargg's test ROMs:

```bash
git clone https://github.com/retrio/gb-test-roms.git roms/gb-test-roms
./gbemu roms/gb-test-roms/cpu_instrs/cpu_instrs.gb
```

The current output shows the ROM header and one log line per executed instruction:

```
Titulo: CPU_INSTRS
Tipo do cartucho: 0x01
Codigo do tamanho da ROM: 0x01
Tamanho do arquivo: 65536 bytes
PC=0100 OP=00 A=01 F=B0 BC=0013 DE=00D8 HL=014D SP=FFFE
...
```

When the emulator hits an opcode that hasn't been implemented yet, it stops and tells you which one:

```
Opcode nao implementado: 0x.. em PC=0x....
```

That's the rhythm of the whole project: each stop points to the next instruction to implement.

## 🗂️ Project structure

```
gbemu/
├── Makefile
├── README.md
└── src/
    ├── main.c      # entry point and main loop
    ├── cart.c/h    # ROM loading and header parsing
    ├── mem.c/h     # memory bus (address map)
    └── cpu.c/h     # registers, flags and instruction execution
```

## 🧠 How it works

### Memory bus

The Game Boy CPU sees a single 64 KB address space, but each range of addresses is a different piece of hardware. The memory bus (`mem_read` / `mem_write`) translates each address to the right region:

| Range | Region |
|---|---|
| `0x0000`–`0x7FFF` | Cartridge ROM |
| `0x8000`–`0x9FFF` | VRAM |
| `0xA000`–`0xBFFF` | Cartridge RAM |
| `0xC000`–`0xDFFF` | WRAM |
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

`cpu_step` fetches an opcode, decodes it, executes it and returns the cycles spent.

## 🗺️ Roadmap

- [x] Load the ROM and read its header
- [x] Memory bus
- [x] CPU structure and first opcodes
- [x] Implement all opcodes (including the `0xCB` prefix)
- [ ] Pass Blargg's `cpu_instrs` tests
- [ ] Timer and interrupts
- [ ] Accurate cycle counting (`instr_timing`)
- [ ] PPU with SDL2 (`dmg-acid2`)
- [ ] Joypad via keyboard
- [ ] MBC1, then MBC3 and MBC5
- [ ] **Run Tetris** 🎉
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