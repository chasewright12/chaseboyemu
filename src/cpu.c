#include "cpu.h"
#include <stdio.h>

static uint8_t rd(Cpu *cpu, uint16_t addr)
{
    return mem_read(cpu->mem, addr);
}

static void wr(Cpu *cpu, uint16_t addr, uint8_t val)
{
    mem_write(cpu->mem, addr, val);
}

static uint8_t fetch8(Cpu *cpu)
{
    return rd(cpu, cpu->pc++);
}

static uint16_t fetch16(Cpu *cpu)
{
    uint8_t lo = fetch8(cpu);
    uint8_t hi = fetch8(cpu);
    return (uint16_t)(hi << 8 | lo); /* little-endian */
}

static uint16_t get_af(const Cpu *cpu) { return (uint16_t)(cpu->a << 8 | cpu->f); }
static uint16_t get_bc(const Cpu *cpu) { return (uint16_t)(cpu->b << 8 | cpu->c); }
static uint16_t get_de(const Cpu *cpu) { return (uint16_t)(cpu->d << 8 | cpu->e); }
static uint16_t get_hl(const Cpu *cpu) { return (uint16_t)(cpu->h << 8 | cpu->l); }

static void set_af(Cpu *cpu, uint16_t v)
{
    cpu->a = v >> 8;
    cpu->f = v & 0xF0;
}
static void set_bc(Cpu *cpu, uint16_t v)
{
    cpu->b = v >> 8;
    cpu->c = v & 0xFF;
}
static void set_de(Cpu *cpu, uint16_t v)
{
    cpu->d = v >> 8;
    cpu->e = v & 0xFF;
}
static void set_hl(Cpu *cpu, uint16_t v)
{
    cpu->h = v >> 8;
    cpu->l = v & 0xFF;
}

/* idx 0..3 = BC, DE, HL, SP */
static uint16_t get_rr(const Cpu *cpu, int idx)
{
    switch (idx)
    {
    case 0:
        return get_bc(cpu);
    case 1:
        return get_de(cpu);
    case 2:
        return get_hl(cpu);
    default:
        return cpu->sp;
    }
}

static void set_rr(Cpu *cpu, int idx, uint16_t v)
{
    switch (idx)
    {
    case 0:
        set_bc(cpu, v);
        break;
    case 1:
        set_de(cpu, v);
        break;
    case 2:
        set_hl(cpu, v);
        break;
    default:
        cpu->sp = v;
        break;
    }
}

static uint8_t read_r(Cpu *cpu, int idx)
{
    switch (idx)
    {
    case 0:
        return cpu->b;
    case 1:
        return cpu->c;
    case 2:
        return cpu->d;
    case 3:
        return cpu->e;
    case 4:
        return cpu->h;
    case 5:
        return cpu->l;
    case 6:
        return rd(cpu, get_hl(cpu));
    default:
        return cpu->a;
    }
}

static void write_r(Cpu *cpu, int idx, uint8_t v)
{
    switch (idx)
    {
    case 0:
        cpu->b = v;
        break;
    case 1:
        cpu->c = v;
        break;
    case 2:
        cpu->d = v;
        break;
    case 3:
        cpu->e = v;
        break;
    case 4:
        cpu->h = v;
        break;
    case 5:
        cpu->l = v;
        break;
    case 6:
        wr(cpu, get_hl(cpu), v);
        break;
    default:
        cpu->a = v;
        break;
    }
}

static void set_flags(Cpu *cpu, int z, int n, int h, int c)
{
    cpu->f = (uint8_t)((z ? FLAG_Z : 0) | (n ? FLAG_N : 0) |
                       (h ? FLAG_H : 0) | (c ? FLAG_C : 0));
}

static int flag_c(const Cpu *cpu) { return (cpu->f & FLAG_C) != 0; }

static void push16(Cpu *cpu, uint16_t v)
{
    wr(cpu, --cpu->sp, (uint8_t)(v >> 8));
    wr(cpu, --cpu->sp, (uint8_t)(v & 0xFF));
}

static uint16_t pop16(Cpu *cpu)
{
    uint8_t lo = rd(cpu, cpu->sp++);
    uint8_t hi = rd(cpu, cpu->sp++);
    return (uint16_t)(hi << 8 | lo);
}

static int check_cond(const Cpu *cpu, int cc)
{
    switch (cc)
    {
    case 0:
        return !(cpu->f & FLAG_Z);
    case 1:
        return (cpu->f & FLAG_Z) != 0;
    case 2:
        return !(cpu->f & FLAG_C);
    default:
        return (cpu->f & FLAG_C) != 0;
    }
}

/* 0 ADD, 1 ADC, 2 SUB, 3 SBC, 4 AND, 5 XOR, 6 OR, 7 CP */
static void alu(Cpu *cpu, int op, uint8_t v)
{
    uint8_t a = cpu->a;
    int carry = (op == 1 || op == 3) ? flag_c(cpu) : 0;
    int r;

    switch (op)
    {
    case 0:
    case 1:
        r = a + v + carry;
        set_flags(cpu, (r & 0xFF) == 0, 0,
                  ((a & 0xF) + (v & 0xF) + carry) > 0xF, r > 0xFF);
        cpu->a = (uint8_t)r;
        break;
    case 2:
    case 3:
    case 7:
        r = a - v - carry;
        set_flags(cpu, (r & 0xFF) == 0, 1,
                  ((a & 0xF) - (v & 0xF) - carry) < 0, r < 0);
        if (op != 7)
            cpu->a = (uint8_t)r;
        break;
    case 4:
        cpu->a = a & v;
        set_flags(cpu, cpu->a == 0, 0, 1, 0);
        break;
    case 5:
        cpu->a = a ^ v;
        set_flags(cpu, cpu->a == 0, 0, 0, 0);
        break;
    default:
        cpu->a = a | v;
        set_flags(cpu, cpu->a == 0, 0, 0, 0);
        break;
    }
}

static uint8_t inc8(Cpu *cpu, uint8_t v)
{
    uint8_t r = (uint8_t)(v + 1);
    set_flags(cpu, r == 0, 0, (v & 0xF) == 0xF, flag_c(cpu));
    return r;
}

static uint8_t dec8(Cpu *cpu, uint8_t v)
{
    uint8_t r = (uint8_t)(v - 1);
    set_flags(cpu, r == 0, 1, (v & 0xF) == 0, flag_c(cpu));
    return r;
}

static void add_hl(Cpu *cpu, uint16_t v)
{
    uint16_t hl = get_hl(cpu);
    uint32_t r = (uint32_t)hl + v;
    set_flags(cpu, cpu->f & FLAG_Z, 0, ((hl & 0xFFF) + (v & 0xFFF)) > 0xFFF, r > 0xFFFF);
    set_hl(cpu, (uint16_t)r);
}

/* SP + e8 (with signal). Used by ADD SP,e8 and LD HL,SP+e8. */
static uint16_t sp_plus_e8(Cpu *cpu, uint8_t e)
{
    uint16_t sp = cpu->sp;
    set_flags(cpu, 0, 0, ((sp & 0xF) + (e & 0xF)) > 0xF, ((sp & 0xFF) + e) > 0xFF);
    return (uint16_t)(sp + (int8_t)e);
}

static void daa(Cpu *cpu)
{
    uint8_t a = cpu->a;
    int n = (cpu->f & FLAG_N) != 0;
    int h = (cpu->f & FLAG_H) != 0;
    int c = flag_c(cpu);

    if (!n)
    {
        if (c || a > 0x99)
        {
            a += 0x60;
            c = 1;
        }
        if (h || (a & 0x0F) > 0x09)
            a += 0x06;
    }
    else
    {
        if (c)
            a -= 0x60;
        if (h)
            a -= 0x06;
    }
    cpu->a = a;
    set_flags(cpu, a == 0, n, 0, c);
}

/* 0 RLC, 1 RRC, 2 RL, 3 RR, 4 SLA, 5 SRA, 6 SWAP, 7 SRL */
static uint8_t cb_shift(Cpu *cpu, int kind, uint8_t v)
{
    int c_in = flag_c(cpu);
    int c_out;
    uint8_t r;

    switch (kind)
    {
    case 0:
        c_out = v >> 7;
        r = (uint8_t)(v << 1 | c_out);
        break;
    case 1:
        c_out = v & 1;
        r = (uint8_t)(v >> 1 | c_out << 7);
        break;
    case 2:
        c_out = v >> 7;
        r = (uint8_t)(v << 1 | c_in);
        break;
    case 3:
        c_out = v & 1;
        r = (uint8_t)(v >> 1 | c_in << 7);
        break;
    case 4:
        c_out = v >> 7;
        r = (uint8_t)(v << 1);
        break;
    case 5:
        c_out = v & 1;
        r = (uint8_t)(v >> 1 | (v & 0x80));
        break;
    case 6:
        c_out = 0;
        r = (uint8_t)(v << 4 | v >> 4);
        break;
    default:
        c_out = v & 1;
        r = (uint8_t)(v >> 1);
        break;
    }
    set_flags(cpu, r == 0, 0, 0, c_out);
    return r;
}

static int exec_cb(Cpu *cpu)
{
    uint8_t op = fetch8(cpu);
    int idx = op & 7;
    int bit = (op >> 3) & 7;
    uint8_t v = read_r(cpu, idx);

    switch (op >> 6)
    {
    case 0:
        write_r(cpu, idx, cb_shift(cpu, bit, v));
        return idx == 6 ? 16 : 8;
    case 1: /* BIT b, r */
        set_flags(cpu, !(v & (1 << bit)), 0, 1, flag_c(cpu));
        return idx == 6 ? 12 : 8;
    case 2: /* RES b, r */
        write_r(cpu, idx, (uint8_t)(v & ~(1 << bit)));
        return idx == 6 ? 16 : 8;
    default: /* SET b, r */
        write_r(cpu, idx, (uint8_t)(v | (1 << bit)));
        return idx == 6 ? 16 : 8;
    }
}

static int execute(Cpu *cpu, uint8_t op)
{
    /* LD r, r' (0x40-0x7F, exceto HALT) */
    if (op >= 0x40 && op <= 0x7F && op != 0x76)
    {
        int dst = (op >> 3) & 7;
        int src = op & 7;
        write_r(cpu, dst, read_r(cpu, src));
        return (dst == 6 || src == 6) ? 8 : 4;
    }

    /* ALU A, r (0x80-0xBF) */
    if (op >= 0x80 && op <= 0xBF)
    {
        int src = op & 7;
        alu(cpu, (op >> 3) & 7, read_r(cpu, src));
        return src == 6 ? 8 : 4;
    }

    switch (op)
    {
    case 0x00:
        return 4; /* NOP */
    case 0x10:    /* STOP */
        fetch8(cpu);
        if (cpu->mem->cgb && (cpu->mem->key1 & 1))
        {
            cpu->mem->double_speed ^= 1;
            cpu->mem->key1 &= (uint8_t)~1;
        }
        return 4;

    case 0x76: /* HALT */
        if (!cpu->ime && (cpu->mem->io[0x0F] & cpu->mem->ie & 0x1F))
        {
            cpu->halt_bug = 1;
        }
        else
        {
            cpu->halted = 1;
        }
        return 4;

    case 0xF3:
        cpu->ime = 0;
        cpu->ei_pending = 0;
        return 4; /* DI */
    case 0xFB:
        cpu->ei_pending = 1;
        return 4; /* EI */

    case 0x02:
        wr(cpu, get_bc(cpu), cpu->a);
        return 8; /* LD (BC), A */
    case 0x12:
        wr(cpu, get_de(cpu), cpu->a);
        return 8; /* LD (DE), A */
    case 0x0A:
        cpu->a = rd(cpu, get_bc(cpu));
        return 8; /* LD A, (BC) */
    case 0x1A:
        cpu->a = rd(cpu, get_de(cpu));
        return 8; /* LD A, (DE) */
    case 0x22:
    { /* LD (HL+), A */
        uint16_t hl = get_hl(cpu);
        wr(cpu, hl, cpu->a);
        set_hl(cpu, (uint16_t)(hl + 1));
        return 8;
    }
    case 0x32:
    { /* LD (HL-), A */
        uint16_t hl = get_hl(cpu);
        wr(cpu, hl, cpu->a);
        set_hl(cpu, (uint16_t)(hl - 1));
        return 8;
    }
    case 0x2A:
    { /* LD A, (HL+) */
        uint16_t hl = get_hl(cpu);
        cpu->a = rd(cpu, hl);
        set_hl(cpu, (uint16_t)(hl + 1));
        return 8;
    }
    case 0x3A:
    { /* LD A, (HL-) */
        uint16_t hl = get_hl(cpu);
        cpu->a = rd(cpu, hl);
        set_hl(cpu, (uint16_t)(hl - 1));
        return 8;
    }
    case 0x08:
    { /* LD (a16), SP */
        uint16_t addr = fetch16(cpu);
        wr(cpu, addr, (uint8_t)(cpu->sp & 0xFF));
        wr(cpu, (uint16_t)(addr + 1), (uint8_t)(cpu->sp >> 8));
        return 20;
    }
    case 0xEA:
        wr(cpu, fetch16(cpu), cpu->a);
        return 16; /* LD (a16), A */
    case 0xFA:
        cpu->a = rd(cpu, fetch16(cpu));
        return 16; /* LD A, (a16) */
    case 0xE0:
        wr(cpu, (uint16_t)(0xFF00 + fetch8(cpu)), cpu->a);
        return 12; /* LDH (a8), A */
    case 0xF0:
        cpu->a = rd(cpu, (uint16_t)(0xFF00 + fetch8(cpu)));
        return 12; /* LDH A, (a8) */
    case 0xE2:
        wr(cpu, (uint16_t)(0xFF00 + cpu->c), cpu->a);
        return 8; /* LD (C), A */
    case 0xF2:
        cpu->a = rd(cpu, (uint16_t)(0xFF00 + cpu->c));
        return 8; /* LD A, (C) */

    /* Operacoes com SP */
    case 0xE8:
        cpu->sp = sp_plus_e8(cpu, fetch8(cpu));
        return 16; /* ADD SP, e8 */
    case 0xF8:
        set_hl(cpu, sp_plus_e8(cpu, fetch8(cpu)));
        return 12; /* LD HL, SP+e8 */
    case 0xF9:
        cpu->sp = get_hl(cpu);
        return 8; /* LD SP, HL */
    case 0x07:
    { /* RLCA */
        int c = cpu->a >> 7;
        cpu->a = (uint8_t)(cpu->a << 1 | c);
        set_flags(cpu, 0, 0, 0, c);
        return 4;
    }
    case 0x0F:
    { /* RRCA */
        int c = cpu->a & 1;
        cpu->a = (uint8_t)(cpu->a >> 1 | c << 7);
        set_flags(cpu, 0, 0, 0, c);
        return 4;
    }
    case 0x17:
    { /* RLA */
        int c = cpu->a >> 7;
        cpu->a = (uint8_t)(cpu->a << 1 | flag_c(cpu));
        set_flags(cpu, 0, 0, 0, c);
        return 4;
    }
    case 0x1F:
    { /* RRA */
        int c = cpu->a & 1;
        cpu->a = (uint8_t)(cpu->a >> 1 | flag_c(cpu) << 7);
        set_flags(cpu, 0, 0, 0, c);
        return 4;
    }

    case 0x27:
        daa(cpu);
        return 4; /* DAA */
    case 0x2F:    /* CPL */
        cpu->a = (uint8_t)~cpu->a;
        set_flags(cpu, cpu->f & FLAG_Z, 1, 1, flag_c(cpu));
        return 4;
    case 0x37:
        set_flags(cpu, cpu->f & FLAG_Z, 0, 0, 1);
        return 4; /* SCF */
    case 0x3F:
        set_flags(cpu, cpu->f & FLAG_Z, 0, 0, !flag_c(cpu));
        return 4; /* CCF */

    case 0x18:
    { /* JR e8 */
        int8_t e = (int8_t)fetch8(cpu);
        cpu->pc = (uint16_t)(cpu->pc + e);
        return 12;
    }
    case 0xC3:
        cpu->pc = fetch16(cpu);
        return 16; /* JP a16 */
    case 0xE9:
        cpu->pc = get_hl(cpu);
        return 4; /* JP HL */
    case 0xCD:
    { /* CALL a16 */
        uint16_t addr = fetch16(cpu);
        push16(cpu, cpu->pc);
        cpu->pc = addr;
        return 24;
    }
    case 0xC9:
        cpu->pc = pop16(cpu);
        return 16; /* RET */
    case 0xD9:
        cpu->pc = pop16(cpu);
        cpu->ime = 1;
        return 16; /* RETI */

    case 0xCB:
        return exec_cb(cpu);
    case 0xD3:
    case 0xDB:
    case 0xDD:
    case 0xE3:
    case 0xE4:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xF4:
    case 0xFC:
    case 0xFD:
        return -1;

    default:
        break;
    }

    if ((op & 0xCF) == 0x01)
    { /* LD rr, d16 */
        set_rr(cpu, (op >> 4) & 3, fetch16(cpu));
        return 12;
    }
    if ((op & 0xCF) == 0x03)
    { /* INC rr */
        int rr = (op >> 4) & 3;
        set_rr(cpu, rr, (uint16_t)(get_rr(cpu, rr) + 1));
        return 8;
    }
    if ((op & 0xCF) == 0x0B)
    { /* DEC rr */
        int rr = (op >> 4) & 3;
        set_rr(cpu, rr, (uint16_t)(get_rr(cpu, rr) - 1));
        return 8;
    }
    if ((op & 0xCF) == 0x09)
    { /* ADD HL, rr */
        add_hl(cpu, get_rr(cpu, (op >> 4) & 3));
        return 8;
    }
    if ((op & 0xC7) == 0x04)
    { /* INC r */
        int idx = (op >> 3) & 7;
        write_r(cpu, idx, inc8(cpu, read_r(cpu, idx)));
        return idx == 6 ? 12 : 4;
    }
    if ((op & 0xC7) == 0x05)
    { /* DEC r */
        int idx = (op >> 3) & 7;
        write_r(cpu, idx, dec8(cpu, read_r(cpu, idx)));
        return idx == 6 ? 12 : 4;
    }
    if ((op & 0xC7) == 0x06)
    { /* LD r, d8 */
        int idx = (op >> 3) & 7;
        write_r(cpu, idx, fetch8(cpu));
        return idx == 6 ? 12 : 8;
    }
    if ((op & 0xE7) == 0x20)
    { /* JR cc, e8 */
        int8_t e = (int8_t)fetch8(cpu);
        if (check_cond(cpu, (op >> 3) & 3))
        {
            cpu->pc = (uint16_t)(cpu->pc + e);
            return 12;
        }
        return 8;
    }
    if ((op & 0xE7) == 0xC0)
    { /* RET cc */
        if (check_cond(cpu, (op >> 3) & 3))
        {
            cpu->pc = pop16(cpu);
            return 20;
        }
        return 8;
    }
    if ((op & 0xE7) == 0xC2)
    { /* JP cc, a16 */
        uint16_t addr = fetch16(cpu);
        if (check_cond(cpu, (op >> 3) & 3))
        {
            cpu->pc = addr;
            return 16;
        }
        return 12;
    }
    if ((op & 0xE7) == 0xC4)
    { /* CALL cc, a16 */
        uint16_t addr = fetch16(cpu);
        if (check_cond(cpu, (op >> 3) & 3))
        {
            push16(cpu, cpu->pc);
            cpu->pc = addr;
            return 24;
        }
        return 12;
    }
    if ((op & 0xCF) == 0xC1)
    { /* POP rr */
        int rr = (op >> 4) & 3;
        uint16_t v = pop16(cpu);
        if (rr == 3)
            set_af(cpu, v);
        else
            set_rr(cpu, rr, v);
        return 12;
    }
    if ((op & 0xCF) == 0xC5)
    { /* PUSH rr */
        int rr = (op >> 4) & 3;
        push16(cpu, rr == 3 ? get_af(cpu) : get_rr(cpu, rr));
        return 16;
    }
    if ((op & 0xC7) == 0xC6)
    { /* ALU A, d8 */
        alu(cpu, (op >> 3) & 7, fetch8(cpu));
        return 8;
    }
    if ((op & 0xC7) == 0xC7)
    { /* RST n */
        push16(cpu, cpu->pc);
        cpu->pc = op & 0x38;
        return 16;
    }

    return -1;
}

static int service_interrupts(Cpu *cpu)
{
    uint8_t pending = cpu->mem->io[0x0F] & cpu->mem->ie & 0x1F;
    if (!pending)
        return 0;

    cpu->halted = 0;
    if (!cpu->ime)
        return 0;

    for (int i = 0; i < 5; i++)
    {
        if (pending & (1 << i))
        {
            cpu->mem->io[0x0F] &= (uint8_t)~(1 << i);
            cpu->ime = 0;
            push16(cpu, cpu->pc);
            cpu->pc = (uint16_t)(0x40 + i * 8);
            return 20;
        }
    }
    return 0;
}

void cpu_init(Cpu *cpu, Mem *mem)
{
    if (mem->cgb)
    {
        cpu->a = 0x11;
        cpu->f = 0x80;
        cpu->b = 0x00;
        cpu->c = 0x00;
        cpu->d = 0xFF;
        cpu->e = 0x56;
        cpu->h = 0x00;
        cpu->l = 0x0D;
    }
    else
    {
        cpu->a = 0x01;
        cpu->f = 0xB0;
        cpu->b = 0x00;
        cpu->c = 0x13;
        cpu->d = 0x00;
        cpu->e = 0xD8;
        cpu->h = 0x01;
        cpu->l = 0x4D;
    }
    cpu->sp = 0xFFFE;
    cpu->pc = 0x0100;
    cpu->ime = 0;
    cpu->ei_pending = 0;
    cpu->halted = 0;
    cpu->halt_bug = 0;
    cpu->cycles = 0;
    cpu->mem = mem;
}

void cpu_log(const Cpu *cpu)
{
    printf("PC=%04X OP=%02X A=%02X F=%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X\n",
           cpu->pc, mem_read(cpu->mem, cpu->pc),
           cpu->a, cpu->f, cpu->b, cpu->c, cpu->d, cpu->e,
           cpu->h, cpu->l, cpu->sp);
}

void cpu_log_doctor(const Cpu *cpu)
{
    printf("A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X "
           "PCMEM:%02X,%02X,%02X,%02X\n",
           cpu->a, cpu->f, cpu->b, cpu->c, cpu->d, cpu->e, cpu->h, cpu->l,
           cpu->sp, cpu->pc,
           mem_read(cpu->mem, cpu->pc),
           mem_read(cpu->mem, (uint16_t)(cpu->pc + 1)),
           mem_read(cpu->mem, (uint16_t)(cpu->pc + 2)),
           mem_read(cpu->mem, (uint16_t)(cpu->pc + 3)));
}

int cpu_step(Cpu *cpu)
{
    int cycles = service_interrupts(cpu);
    if (cycles)
    {
        cpu->cycles += (uint64_t)cycles;
        return cycles;
    }

    if (cpu->halted)
    {
        cpu->cycles += 4;
        return 4;
    }

    uint8_t op = rd(cpu, cpu->pc);
    if (cpu->halt_bug)
    {
        cpu->halt_bug = 0;
    }
    else
    {
        cpu->pc++;
    }

    cycles = execute(cpu, op);
    if (cycles < 0)
    {
        cpu->pc--;
        fprintf(stderr, "Opcode invalido: 0x%02X em PC=0x%04X\n", op, cpu->pc);
        return -1;
    }

    if (cpu->ei_pending && op != 0xFB)
    {
        cpu->ime = 1;
        cpu->ei_pending = 0;
    }

    cpu->cycles += (uint64_t)cycles;
    return cycles;
}