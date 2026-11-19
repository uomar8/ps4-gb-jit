#include "sm83_interp.h"
#include "../mem/bus.h"
#include "../../debug/dbg_stub.h"

static inline uint16_t get_af(const GbCpu *c) { return (uint16_t)(((uint16_t)c->a << 8) | (c->f & 0xF0U)); }
static inline uint16_t get_bc(const GbCpu *c) { return (uint16_t)(((uint16_t)c->b << 8) | c->c); }
static inline uint16_t get_de(const GbCpu *c) { return (uint16_t)(((uint16_t)c->d << 8) | c->e); }
static inline uint16_t get_hl(const GbCpu *c) { return (uint16_t)(((uint16_t)c->h << 8) | c->l); }

static inline void set_af(GbCpu *c, uint16_t v) { c->a = (uint8_t)(v >> 8); c->f = (uint8_t)(v & 0xF0U); }
static inline void set_bc(GbCpu *c, uint16_t v) { c->b = (uint8_t)(v >> 8); c->c = (uint8_t)(v & 0xFFU); }
static inline void set_de(GbCpu *c, uint16_t v) { c->d = (uint8_t)(v >> 8); c->e = (uint8_t)(v & 0xFFU); }
static inline void set_hl(GbCpu *c, uint16_t v) { c->h = (uint8_t)(v >> 8); c->l = (uint8_t)(v & 0xFFU); }

static inline uint8_t fetch8(GbMachine *m) {
    uint8_t v = bus_read8(m, m->cpu.pc);
    if (m->cpu.halt_bug) {
        m->cpu.halt_bug = 0;
    } else {
        m->cpu.pc++;
    }
    return v;
}

static inline uint16_t fetch16(GbMachine *m) {
    uint16_t lo = fetch8(m);
    uint16_t hi = fetch8(m);
    return (uint16_t)(lo | (hi << 8));
}

static inline void push16(GbMachine *m, uint16_t val) {
    m->cpu.sp = (uint16_t)(m->cpu.sp - 2U);
    bus_write16(m, m->cpu.sp, val);
}

static inline uint16_t pop16(GbMachine *m) {
    uint16_t v = bus_read16(m, m->cpu.sp);
    m->cpu.sp = (uint16_t)(m->cpu.sp + 2U);
    return v;
}

static uint8_t read_r8(GbMachine *m, uint8_t idx) {
    switch (idx & 7U) {
        case 0: return m->cpu.b;
        case 1: return m->cpu.c;
        case 2: return m->cpu.d;
        case 3: return m->cpu.e;
        case 4: return m->cpu.h;
        case 5: return m->cpu.l;
        case 6: return bus_read8(m, get_hl(&m->cpu));
        case 7: return m->cpu.a;
        default: return 0xFFU;
    }
}

static void write_r8(GbMachine *m, uint8_t idx, uint8_t val) {
    switch (idx & 7U) {
        case 0: m->cpu.b = val; break;
        case 1: m->cpu.c = val; break;
        case 2: m->cpu.d = val; break;
        case 3: m->cpu.e = val; break;
        case 4: m->cpu.h = val; break;
        case 5: m->cpu.l = val; break;
        case 6: bus_write8(m, get_hl(&m->cpu), val); break;
        case 7: m->cpu.a = val; break;
        default: break;
    }
}

static void alu_add(GbCpu *c, uint8_t val, uint8_t carry) {
    uint32_t res = (uint32_t)c->a + val + carry;
    uint8_t f = 0;
    if ((res & 0xFFU) == 0) f |= SM83_FLAG_Z;
    if (((c->a & 0x0FU) + (val & 0x0FU) + carry) > 0x0FU) f |= SM83_FLAG_H;
    if (res > 0xFFU) f |= SM83_FLAG_C;
    c->a = (uint8_t)res;
    c->f = f;
}

static void alu_sub(GbCpu *c, uint8_t val, uint8_t carry, bool store_a) {
    int32_t res = (int32_t)c->a - (int32_t)val - (int32_t)carry;
    uint8_t f = SM83_FLAG_N;
    if ((res & 0xFF) == 0) f |= SM83_FLAG_Z;
    if (((int32_t)(c->a & 0x0FU) - (int32_t)(val & 0x0FU) - (int32_t)carry) < 0) f |= SM83_FLAG_H;
    if (res < 0) f |= SM83_FLAG_C;
    if (store_a) c->a = (uint8_t)res;
    c->f = f;
}

static uint8_t alu_inc8(GbCpu *c, uint8_t v) {
    uint8_t r = (uint8_t)(v + 1U);
    uint8_t f = (uint8_t)(c->f & SM83_FLAG_C);
    if (r == 0) f |= SM83_FLAG_Z;
    if (((v & 0x0FU) + 1U) > 0x0FU) f |= SM83_FLAG_H;
    c->f = f;
    return r;
}

static uint8_t alu_dec8(GbCpu *c, uint8_t v) {
    uint8_t r = (uint8_t)(v - 1U);
    uint8_t f = (uint8_t)((c->f & SM83_FLAG_C) | SM83_FLAG_N);
    if (r == 0) f |= SM83_FLAG_Z;
    if ((v & 0x0FU) == 0) f |= SM83_FLAG_H;
    c->f = f;
    return r;
}

static void alu_add_hl(GbCpu *c, uint16_t val) {
    uint16_t hl = get_hl(c);
    uint32_t res = (uint32_t)hl + val;
    uint8_t f = (uint8_t)(c->f & SM83_FLAG_Z);
    if (((hl & 0x0FFFU) + (val & 0x0FFFU)) > 0x0FFFU) f |= SM83_FLAG_H;
    if (res > 0xFFFFU) f |= SM83_FLAG_C;
    set_hl(c, (uint16_t)res);
    c->f = f;
}

static uint16_t alu_add_sp_r8(GbCpu *c, int8_t r8) {
    uint16_t sp = c->sp;
    uint8_t u = (uint8_t)r8;
    uint8_t f = 0;
    if (((sp & 0x0FU) + (u & 0x0FU)) > 0x0FU) f |= SM83_FLAG_H;
    if (((sp & 0xFFU) + u) > 0xFFU) f |= SM83_FLAG_C;
    c->f = f;
    return (uint16_t)(sp + (int16_t)r8);
}

static bool cond_met(const GbCpu *c, uint8_t cc) {
    switch (cc & 3U) {
        case 0: return (c->f & SM83_FLAG_Z) == 0; /* NZ */
        case 1: return (c->f & SM83_FLAG_Z) != 0; /* Z  */
        case 2: return (c->f & SM83_FLAG_C) == 0; /* NC */
        case 3: return (c->f & SM83_FLAG_C) != 0; /* C  */
        default: return false;
    }
}

static uint32_t exec_cb(GbMachine *m) {
    uint8_t op = fetch8(m);
    uint8_t reg = op & 7U;
    uint8_t bit = (op >> 3) & 7U;
    uint8_t grp = op >> 6;
    uint8_t v = read_r8(m, reg);
    GbCpu *c = &m->cpu;
    bool is_hl = (reg == 6U);

    if (grp == 0U) {
        uint8_t res = v;
        uint8_t carry = 0;
        switch (bit) {
            case 0: /* RLC */
                carry = (v >> 7) & 1U;
                res = (uint8_t)((v << 1) | carry);
                break;
            case 1: /* RRC */
                carry = v & 1U;
                res = (uint8_t)((v >> 1) | (carry << 7));
                break;
            case 2: /* RL */
                carry = (v >> 7) & 1U;
                res = (uint8_t)((v << 1) | ((c->f & SM83_FLAG_C) ? 1U : 0U));
                break;
            case 3: /* RR */
                carry = v & 1U;
                res = (uint8_t)((v >> 1) | ((c->f & SM83_FLAG_C) ? 0x80U : 0U));
                break;
            case 4: /* SLA */
                carry = (v >> 7) & 1U;
                res = (uint8_t)(v << 1);
                break;
            case 5: /* SRA */
                carry = v & 1U;
                res = (uint8_t)((v >> 1) | (v & 0x80U));
                break;
            case 6: /* SWAP */
                carry = 0;
                res = (uint8_t)((v << 4) | (v >> 4));
                break;
            case 7: /* SRL */
                carry = v & 1U;
                res = (uint8_t)(v >> 1);
                break;
            default: break;
        }
        c->f = (uint8_t)((res == 0 ? SM83_FLAG_Z : 0U) | (carry ? SM83_FLAG_C : 0U));
        write_r8(m, reg, res);
        return is_hl ? 16U : 8U;
    }

    if (grp == 1U) { /* BIT b, r */
        uint8_t f = (uint8_t)((c->f & SM83_FLAG_C) | SM83_FLAG_H);
        if ((v & (1U << bit)) == 0) f |= SM83_FLAG_Z;
        c->f = f;
        return is_hl ? 12U : 8U;
    }

    if (grp == 2U) { /* RES b, r */
        write_r8(m, reg, (uint8_t)(v & ~(1U << bit)));
        return is_hl ? 16U : 8U;
    }

    /* grp == 3: SET b, r */
    write_r8(m, reg, (uint8_t)(v | (1U << bit)));
    return is_hl ? 16U : 8U;
}

uint32_t sm83_check_interrupts(GbMachine *m) {
    uint8_t pending = (uint8_t)(m->if_reg & m->ie_reg & 0x1FU);
    if (pending == 0) {
        return 0;
    }
    m->cpu.halted = 0;
    if (!m->cpu.ime) {
        return 0;
    }

    for (uint8_t i = 0; i < 5; i++) {
        if (pending & (1U << i)) {
            m->cpu.ime = 0;
            m->if_reg &= (uint8_t)~(1U << i);
            push16(m, m->cpu.pc);
            m->cpu.pc = (uint16_t)(0x0040U + i * 8U);
            return 20U;
        }
    }
    return 0;
}

uint32_t sm83_interp_step(GbMachine *m) {
    uint32_t irq_cyc = sm83_check_interrupts(m);
    if (irq_cyc > 0) {
        return irq_cyc;
    }

    if (m->cpu.halted) {
        return 4U;
    }

    bool enable_ime_after = (m->cpu.ime_delay != 0);
    if (m->cpu.ime_delay) {
        m->cpu.ime_delay = 0;
    }

    GbCpu *c = &m->cpu;
    uint8_t op = fetch8(m);
    uint32_t cycles = 4U;

    /* 0x40..0x7F: LD r, r' (except 0x76 HALT) */
    if (op >= 0x40U && op <= 0x7FU) {
        if (op == 0x76U) {
            if (!c->ime && (m->if_reg & m->ie_reg & 0x1FU) != 0) {
                c->halt_bug = 1;
            } else {
                c->halted = 1;
            }
            cycles = 4U;
        } else {
            uint8_t dst = (op >> 3) & 7U;
            uint8_t src = op & 7U;
            write_r8(m, dst, read_r8(m, src));
            cycles = (dst == 6U || src == 6U) ? 8U : 4U;
        }
        if (enable_ime_after) c->ime = 1;
        return cycles;
    }

    /* 0x80..0xBF: ALU A, r */
    if (op >= 0x80U && op <= 0xBFU) {
        uint8_t src = op & 7U;
        uint8_t val = read_r8(m, src);
        switch ((op >> 3) & 7U) {
            case 0: alu_add(c, val, 0); break;
            case 1: alu_add(c, val, (c->f & SM83_FLAG_C) ? 1U : 0U); break;
            case 2: alu_sub(c, val, 0, true); break;
            case 3: alu_sub(c, val, (c->f & SM83_FLAG_C) ? 1U : 0U, true); break;
            case 4: c->a &= val; c->f = (uint8_t)((c->a == 0 ? SM83_FLAG_Z : 0U) | SM83_FLAG_H); break;
            case 5: c->a ^= val; c->f = (uint8_t)(c->a == 0 ? SM83_FLAG_Z : 0U); break;
            case 6: c->a |= val; c->f = (uint8_t)(c->a == 0 ? SM83_FLAG_Z : 0U); break;
            case 7: alu_sub(c, val, 0, false); break;
            default: break;
        }
        cycles = (src == 6U) ? 8U : 4U;
        if (enable_ime_after) c->ime = 1;
        return cycles;
    }

    switch (op) {
        case 0x00U: cycles = 4U; break; /* NOP */
        case 0x10U: (void)fetch8(m); c->stopped = 1; cycles = 4U; break; /* STOP */

        /* LD rr, d16 */
        case 0x01U: set_bc(c, fetch16(m)); cycles = 12U; break;
        case 0x11U: set_de(c, fetch16(m)); cycles = 12U; break;
        case 0x21U: set_hl(c, fetch16(m)); cycles = 12U; break;
        case 0x31U: c->sp = fetch16(m);    cycles = 12U; break;

        /* LD (rr), A & LD A, (rr) */
        case 0x02U: bus_write8(m, get_bc(c), c->a); cycles = 8U; break;
        case 0x12U: bus_write8(m, get_de(c), c->a); cycles = 8U; break;
        case 0x22U: { uint16_t hl = get_hl(c); bus_write8(m, hl, c->a); set_hl(c, (uint16_t)(hl + 1U)); cycles = 8U; break; }
        case 0x32U: { uint16_t hl = get_hl(c); bus_write8(m, hl, c->a); set_hl(c, (uint16_t)(hl - 1U)); cycles = 8U; break; }
        case 0x0AU: c->a = bus_read8(m, get_bc(c)); cycles = 8U; break;
        case 0x1AU: c->a = bus_read8(m, get_de(c)); cycles = 8U; break;
        case 0x2AU: { uint16_t hl = get_hl(c); c->a = bus_read8(m, hl); set_hl(c, (uint16_t)(hl + 1U)); cycles = 8U; break; }
        case 0x3AU: { uint16_t hl = get_hl(c); c->a = bus_read8(m, hl); set_hl(c, (uint16_t)(hl - 1U)); cycles = 8U; break; }

        /* LD (a16), SP */
        case 0x08U: bus_write16(m, fetch16(m), c->sp); cycles = 20U; break;

        /* INC / DEC rr */
        case 0x03U: set_bc(c, (uint16_t)(get_bc(c) + 1U)); cycles = 8U; break;
        case 0x13U: set_de(c, (uint16_t)(get_de(c) + 1U)); cycles = 8U; break;
        case 0x23U: set_hl(c, (uint16_t)(get_hl(c) + 1U)); cycles = 8U; break;
        case 0x33U: c->sp = (uint16_t)(c->sp + 1U);        cycles = 8U; break;
        case 0x0BU: set_bc(c, (uint16_t)(get_bc(c) - 1U)); cycles = 8U; break;
        case 0x1BU: set_de(c, (uint16_t)(get_de(c) - 1U)); cycles = 8U; break;
        case 0x2BU: set_hl(c, (uint16_t)(get_hl(c) - 1U)); cycles = 8U; break;
        case 0x3BU: c->sp = (uint16_t)(c->sp - 1U);        cycles = 8U; break;

        /* ADD HL, rr */
        case 0x09U: alu_add_hl(c, get_bc(c)); cycles = 8U; break;
        case 0x19U: alu_add_hl(c, get_de(c)); cycles = 8U; break;
        case 0x29U: alu_add_hl(c, get_hl(c)); cycles = 8U; break;
        case 0x39U: alu_add_hl(c, c->sp);     cycles = 8U; break;

        /* INC r8 */
        case 0x04U: case 0x0CU: case 0x14U: case 0x1CU:
        case 0x24U: case 0x2CU: case 0x34U: case 0x3CU: {
            uint8_t r = (op >> 3) & 7U;
            write_r8(m, r, alu_inc8(c, read_r8(m, r)));
            cycles = (r == 6U) ? 12U : 4U;
            break;
        }
        /* DEC r8 */
        case 0x05U: case 0x0DU: case 0x15U: case 0x1DU:
        case 0x25U: case 0x2DU: case 0x35U: case 0x3DU: {
            uint8_t r = (op >> 3) & 7U;
            write_r8(m, r, alu_dec8(c, read_r8(m, r)));
            cycles = (r == 6U) ? 12U : 4U;
            break;
        }
        /* LD r8, d8 */
        case 0x06U: case 0x0EU: case 0x16U: case 0x1EU:
        case 0x26U: case 0x2EU: case 0x36U: case 0x3EU: {
            uint8_t r = (op >> 3) & 7U;
            write_r8(m, r, fetch8(m));
            cycles = (r == 6U) ? 12U : 8U;
            break;
        }

        /* Rotate A & Misc flags */
        case 0x07U: { /* RLCA */
            uint8_t carry = (c->a >> 7) & 1U;
            c->a = (uint8_t)((c->a << 1) | carry);
            c->f = carry ? SM83_FLAG_C : 0U;
            cycles = 4U;
            break;
        }
        case 0x0FU: { /* RRCA */
            uint8_t carry = c->a & 1U;
            c->a = (uint8_t)((c->a >> 1) | (carry << 7));
            c->f = carry ? SM83_FLAG_C : 0U;
            cycles = 4U;
            break;
        }
        case 0x17U: { /* RLA */
            uint8_t carry = (c->a >> 7) & 1U;
            c->a = (uint8_t)((c->a << 1) | ((c->f & SM83_FLAG_C) ? 1U : 0U));
            c->f = carry ? SM83_FLAG_C : 0U;
            cycles = 4U;
            break;
        }
        case 0x1FU: { /* RRA */
            uint8_t carry = c->a & 1U;
            c->a = (uint8_t)((c->a >> 1) | ((c->f & SM83_FLAG_C) ? 0x80U : 0U));
            c->f = carry ? SM83_FLAG_C : 0U;
            cycles = 4U;
            break;
        }
        case 0x27U: { /* DAA */
            uint8_t adj = 0;
            uint8_t cf = (c->f & SM83_FLAG_C) ? 1U : 0U;
            if ((c->f & SM83_FLAG_N) == 0) {
                if ((c->f & SM83_FLAG_H) || (c->a & 0x0FU) > 9U) adj |= 0x06U;
                if (cf || c->a > 0x99U) { adj |= 0x60U; cf = 1U; }
                c->a = (uint8_t)(c->a + adj);
            } else {
                if (c->f & SM83_FLAG_H) adj |= 0x06U;
                if (cf) adj |= 0x60U;
                c->a = (uint8_t)(c->a - adj);
            }
            c->f = (uint8_t)((c->f & SM83_FLAG_N) | (c->a == 0 ? SM83_FLAG_Z : 0U) | (cf ? SM83_FLAG_C : 0U));
            cycles = 4U;
            break;
        }
        case 0x2FU: c->a = (uint8_t)~c->a; c->f |= (SM83_FLAG_N | SM83_FLAG_H); cycles = 4U; break; /* CPL */
        case 0x37U: c->f = (uint8_t)((c->f & SM83_FLAG_Z) | SM83_FLAG_C); cycles = 4U; break; /* SCF */
        case 0x3FU: c->f = (uint8_t)((c->f & SM83_FLAG_Z) | ((c->f & SM83_FLAG_C) ? 0U : SM83_FLAG_C)); cycles = 4U; break; /* CCF */

        /* JR */
        case 0x18U: { int8_t r8 = (int8_t)fetch8(m); c->pc = (uint16_t)(c->pc + r8); cycles = 12U; break; }
        case 0x20U: case 0x28U: case 0x30U: case 0x38U: {
            int8_t r8 = (int8_t)fetch8(m);
            if (cond_met(c, (op >> 3) & 3U)) {
                c->pc = (uint16_t)(c->pc + r8);
                cycles = 12U;
            } else {
                cycles = 8U;
            }
            break;
        }

        /* RET / RETI / JP / CALL / RST */
        case 0xC9U: c->pc = pop16(m); cycles = 16U; break;
        case 0xD9U: c->pc = pop16(m); c->ime = 1; cycles = 16U; break;
        case 0xC0U: case 0xC8U: case 0xD0U: case 0xD8U:
            if (cond_met(c, (op >> 3) & 3U)) { c->pc = pop16(m); cycles = 20U; }
            else { cycles = 8U; }
            break;

        case 0xC3U: c->pc = fetch16(m); cycles = 16U; break;
        case 0xE9U: c->pc = get_hl(c);  cycles = 4U;  break;
        case 0xC2U: case 0xCAU: case 0xD2U: case 0xDAU: {
            uint16_t addr = fetch16(m);
            if (cond_met(c, (op >> 3) & 3U)) { c->pc = addr; cycles = 16U; }
            else { cycles = 12U; }
            break;
        }

        case 0xCDU: { uint16_t addr = fetch16(m); push16(m, c->pc); c->pc = addr; cycles = 24U; break; }
        case 0xC4U: case 0xCCU: case 0xD4U: case 0xDCU: {
            uint16_t addr = fetch16(m);
            if (cond_met(c, (op >> 3) & 3U)) { push16(m, c->pc); c->pc = addr; cycles = 24U; }
            else { cycles = 12U; }
            break;
        }

        case 0xC7U: case 0xCFU: case 0xD7U: case 0xDFU:
        case 0xE7U: case 0xEFU: case 0xF7U: case 0xFFU:
            push16(m, c->pc);
            c->pc = (uint16_t)(op & 0x38U);
            cycles = 16U;
            break;

        /* PUSH / POP */
        case 0xC5U: push16(m, get_bc(c)); cycles = 16U; break;
        case 0xD5U: push16(m, get_de(c)); cycles = 16U; break;
        case 0xE5U: push16(m, get_hl(c)); cycles = 16U; break;
        case 0xF5U: push16(m, get_af(c)); cycles = 16U; break;
        case 0xC1U: set_bc(c, pop16(m));  cycles = 12U; break;
        case 0xD1U: set_de(c, pop16(m));  cycles = 12U; break;
        case 0xE1U: set_hl(c, pop16(m));  cycles = 12U; break;
        case 0xF1U: set_af(c, pop16(m));  cycles = 12U; break;

        /* ALU A, d8 */
        case 0xC6U: alu_add(c, fetch8(m), 0); cycles = 8U; break;
        case 0xCEU: alu_add(c, fetch8(m), (c->f & SM83_FLAG_C) ? 1U : 0U); cycles = 8U; break;
        case 0xD6U: alu_sub(c, fetch8(m), 0, true); cycles = 8U; break;
        case 0xDEU: alu_sub(c, fetch8(m), (c->f & SM83_FLAG_C) ? 1U : 0U, true); cycles = 8U; break;
        case 0xE6U: c->a &= fetch8(m); c->f = (uint8_t)((c->a == 0 ? SM83_FLAG_Z : 0U) | SM83_FLAG_H); cycles = 8U; break;
        case 0xEEU: c->a ^= fetch8(m); c->f = (uint8_t)(c->a == 0 ? SM83_FLAG_Z : 0U); cycles = 8U; break;
        case 0xF6U: c->a |= fetch8(m); c->f = (uint8_t)(c->a == 0 ? SM83_FLAG_Z : 0U); cycles = 8U; break;
        case 0xFEU: alu_sub(c, fetch8(m), 0, false); cycles = 8U; break;

        /* High RAM / absolute loads & SP ops */
        case 0xE0U: bus_write8(m, (uint16_t)(0xFF00U + fetch8(m)), c->a); cycles = 12U; break;
        case 0xF0U: c->a = bus_read8(m, (uint16_t)(0xFF00U + fetch8(m))); cycles = 12U; break;
        case 0xE2U: bus_write8(m, (uint16_t)(0xFF00U + c->c), c->a); cycles = 8U; break;
        case 0xF2U: c->a = bus_read8(m, (uint16_t)(0xFF00U + c->c)); cycles = 8U; break;
        case 0xEAU: bus_write8(m, fetch16(m), c->a); cycles = 16U; break;
        case 0xFAU: c->a = bus_read8(m, fetch16(m)); cycles = 16U; break;
        case 0xE8U: c->sp = alu_add_sp_r8(c, (int8_t)fetch8(m)); cycles = 16U; break;
        case 0xF8U: set_hl(c, alu_add_sp_r8(c, (int8_t)fetch8(m))); cycles = 12U; break;
        case 0xF9U: c->sp = get_hl(c); cycles = 8U; break;

        case 0xF3U: c->ime = 0; c->ime_delay = 0; cycles = 4U; break; /* DI */
        case 0xFBU: c->ime_delay = 1; cycles = 4U; break;             /* EI */
        case 0xCBU: cycles = exec_cb(m); break;

        default:
            dbg_stub_record("OPCODE", op, "ILLEGAL_SM83_OPCODE");
            cycles = 4U;
            break;
    }

    if (enable_ime_after && op != 0xF3U) {
        c->ime = 1;
    }
    return cycles;
}
