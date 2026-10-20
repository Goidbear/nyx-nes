#include "cpu.h"
#include <cstdio>
#include "bus.h"
#include "savestate.h"

CPU::CPU() {}
CPU::~CPU() {}

Byte CPU::read(Word addr) { return bus->cpuRead(addr); }
void CPU::write(Word addr, Byte data) { bus->cpuWrite(addr, data); }

Byte CPU::fetch() { return read(pc++); }

Word CPU::fetchWord() {
    Word lo = fetch();
    Word hi = fetch();
    return (hi << 8) | lo;
}

Word CPU::readWord(Word addr) {
    Word lo = read(addr);
    Word hi = read((addr & 0xFF00) | ((addr + 1) & 0x00FF));
    return (hi << 8) | lo;
}

// careful: readWord above is a bit off; keep a dedicated ptr helper
static Word readPtr(Bus* bus, Word addr) {
    Byte lo = bus->cpuRead(addr);
    Byte hi = bus->cpuRead((addr & 0xFF00) | ((addr + 1) & 0x00FF));
    return (hi << 8) | lo;
}

void CPU::SetFlag(FLAGS f, bool v) {
    if (v) status |= f; else status &= ~f;
}
bool CPU::GetFlag(FLAGS f) const { return (status & f) != 0; }

void CPU::push(Byte v) { write(0x0100 | stkp, v); stkp--; }
Byte CPU::pop() { stkp++; return read(0x0100 | stkp); }

void CPU::Reset() {
    a = x = y = 0;
    stkp = 0xFD;
    status = 0x24;
    pc = read(0xFFFC) | (read(0xFFFD) << 8);
}

void CPU::IRQ() { irq_pending = true; }
void CPU::NMI() { nmi_pending = true; }

Word CPU::addr_imm() { return pc++; }
Word CPU::addr_zp0() { return fetch(); }
Word CPU::addr_zpx() { return (fetch() + x) & 0xFF; }
Word CPU::addr_zpy() { return (fetch() + y) & 0xFF; }
Word CPU::addr_abs() { return fetchWord(); }
Word CPU::addr_absx() { return fetchWord() + x; }
Word CPU::addr_absy() { return fetchWord() + y; }
Word CPU::addr_indx() {
    Byte zp = (fetch() + x) & 0xFF;
    return read(zp) | (read((zp + 1) & 0xFF) << 8);
}
Word CPU::addr_indy() {
    Byte zp = fetch();
    Word base = read(zp) | (read((zp + 1) & 0xFF) << 8);
    return base + y;
}

void CPU::ADC(Byte v) {
    Word sum = a + v + (GetFlag(C) ? 1 : 0);
    SetFlag(C, sum > 0xFF);
    SetFlag(V, ((~(a ^ v) & (a ^ sum)) & 0x80) != 0);
    a = sum & 0xFF;
    SetFlag(Z, a == 0);
    SetFlag(N, a & 0x80);
}
void CPU::SBC(Byte v) { ADC(v ^ 0xFF); }

void CPU::CMP(Byte r, Byte v) {
    Word d = r - v;
    SetFlag(C, r >= v);
    SetFlag(Z, (d & 0xFF) == 0);
    SetFlag(N, d & 0x80);
}

void CPU::branch(bool cond) {
    sByte offset = (sByte)fetch();
    if (cond) pc += offset;
}

void CPU::execute(Byte op) {
    switch (op) {
    // LDA
    case 0xA9: a = read(addr_imm()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xA5: a = read(addr_zp0()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xB5: a = read(addr_zpx()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xAD: a = read(addr_abs()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xBD: a = read(addr_absx()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xB9: a = read(addr_absy()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xA1: a = read(addr_indx()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;
    case 0xB1: a = read(addr_indy()); SetFlag(Z, a==0); SetFlag(N, a&0x80); break;

    // LDX
    case 0xA2: x = read(addr_imm()); SetFlag(Z, x==0); SetFlag(N, x&0x80); break;
    case 0xA6: x = read(addr_zp0()); SetFlag(Z, x==0); SetFlag(N, x&0x80); break;
    case 0xB6: x = read(addr_zpy()); SetFlag(Z, x==0); SetFlag(N, x&0x80); break;
    case 0xAE: x = read(addr_abs()); SetFlag(Z, x==0); SetFlag(N, x&0x80); break;
    case 0xBE: x = read(addr_absy()); SetFlag(Z, x==0); SetFlag(N, x&0x80); break;

    // LDY
    case 0xA0: y = read(addr_imm()); SetFlag(Z, y==0); SetFlag(N, y&0x80); break;
    case 0xA4: y = read(addr_zp0()); SetFlag(Z, y==0); SetFlag(N, y&0x80); break;
    case 0xB4: y = read(addr_zpx()); SetFlag(Z, y==0); SetFlag(N, y&0x80); break;
    case 0xAC: y = read(addr_abs()); SetFlag(Z, y==0); SetFlag(N, y&0x80); break;
    case 0xBC: y = read(addr_absx()); SetFlag(Z, y==0); SetFlag(N, y&0x80); break;

    // STA
    case 0x85: write(addr_zp0(), a); break;
    case 0x95: write(addr_zpx(), a); break;
    case 0x8D: write(addr_abs(), a); break;
    case 0x9D: write(addr_absx(), a); break;
    case 0x99: write(addr_absy(), a); break;
    case 0x81: write(addr_indx(), a); break;
    case 0x91: write(addr_indy(), a); break;

    // STX / STY
    case 0x86: write(addr_zp0(), x); break;
    case 0x96: write(addr_zpy(), x); break;
    case 0x8E: write(addr_abs(), x); break;
    case 0x84: write(addr_zp0(), y); break;
    case 0x94: write(addr_zpx(), y); break;
    case 0x8C: write(addr_abs(), y); break;

    // transfers
    case 0xAA: x = a; SetFlag(Z, x==0); SetFlag(N, x&0x80); break; // TAX
    case 0xA8: y = a; SetFlag(Z, y==0); SetFlag(N, y&0x80); break; // TAY
    case 0x8A: a = x; SetFlag(Z, a==0); SetFlag(N, a&0x80); break; // TXA
    case 0x98: a = y; SetFlag(Z, a==0); SetFlag(N, a&0x80); break; // TYA
    case 0xBA: x = stkp; SetFlag(Z, x==0); SetFlag(N, x&0x80); break; // TSX
    case 0x9A: stkp = x; break; // TXS

    // stack
    case 0x48: push(a); break;               // PHA
    case 0x08: push(status | B | U); break;  // PHP
    case 0x68: a = pop(); SetFlag(Z, a==0); SetFlag(N, a&0x80); break; // PLA
    case 0x28: status = (pop() & ~B) | U; break; // PLP

    // arithmetic
    case 0x69: ADC(read(addr_imm())); break;
    case 0x65: ADC(read(addr_zp0())); break;
    case 0x75: ADC(read(addr_zpx())); break;
    case 0x6D: ADC(read(addr_abs())); break;
    case 0x7D: ADC(read(addr_absx())); break;
    case 0x79: ADC(read(addr_absy())); break;
    case 0x61: ADC(read(addr_indx())); break;
    case 0x71: ADC(read(addr_indy())); break;

    case 0xE9: SBC(read(addr_imm())); break;
    case 0xE5: SBC(read(addr_zp0())); break;
    case 0xF5: SBC(read(addr_zpx())); break;
    case 0xED: SBC(read(addr_abs())); break;
    case 0xFD: SBC(read(addr_absx())); break;
    case 0xF9: SBC(read(addr_absy())); break;
    case 0xE1: SBC(read(addr_indx())); break;
    case 0xF1: SBC(read(addr_indy())); break;

    // compare
    case 0xC9: CMP(a, read(addr_imm())); break;
    case 0xC5: CMP(a, read(addr_zp0())); break;
    case 0xD5: CMP(a, read(addr_zpx())); break;
    case 0xCD: CMP(a, read(addr_abs())); break;
    case 0xDD: CMP(a, read(addr_absx())); break;
    case 0xD9: CMP(a, read(addr_absy())); break;
    case 0xC1: CMP(a, read(addr_indx())); break;
    case 0xD1: CMP(a, read(addr_indy())); break;

    case 0xE0: CMP(x, read(addr_imm())); break; // CPX
    case 0xE4: CMP(x, read(addr_zp0())); break;
    case 0xEC: CMP(x, read(addr_abs())); break;
    case 0xC0: CMP(y, read(addr_imm())); break; // CPY
    case 0xC4: CMP(y, read(addr_zp0())); break;
    case 0xCC: CMP(y, read(addr_abs())); break;

    // inc / dec
    case 0xE6: { Word ad = addr_zp0(); Byte v = read(ad)+1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xF6: { Word ad = addr_zpx(); Byte v = read(ad)+1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xEE: { Word ad = addr_abs(); Byte v = read(ad)+1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xFE: { Word ad = addr_absx(); Byte v = read(ad)+1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xC6: { Word ad = addr_zp0(); Byte v = read(ad)-1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xD6: { Word ad = addr_zpx(); Byte v = read(ad)-1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xCE: { Word ad = addr_abs(); Byte v = read(ad)-1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0xDE: { Word ad = addr_absx(); Byte v = read(ad)-1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;

    case 0xE8: x++; SetFlag(Z,x==0); SetFlag(N,x&0x80); break; // INX
    case 0xCA: x--; SetFlag(Z,x==0); SetFlag(N,x&0x80); break; // DEX
    case 0xC8: y++; SetFlag(Z,y==0); SetFlag(N,y&0x80); break; // INY
    case 0x88: y--; SetFlag(Z,y==0); SetFlag(N,y&0x80); break; // DEY

    // logic
    case 0x29: a &= read(addr_imm()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x25: a &= read(addr_zp0()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x35: a &= read(addr_zpx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x2D: a &= read(addr_abs()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x3D: a &= read(addr_absx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x39: a &= read(addr_absy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x21: a &= read(addr_indx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x31: a &= read(addr_indy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;

    case 0x09: a |= read(addr_imm()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x05: a |= read(addr_zp0()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x15: a |= read(addr_zpx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x0D: a |= read(addr_abs()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x1D: a |= read(addr_absx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x19: a |= read(addr_absy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x01: a |= read(addr_indx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x11: a |= read(addr_indy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;

    case 0x49: a ^= read(addr_imm()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x45: a ^= read(addr_zp0()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x55: a ^= read(addr_zpx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x4D: a ^= read(addr_abs()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x5D: a ^= read(addr_absx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x59: a ^= read(addr_absy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x41: a ^= read(addr_indx()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;
    case 0x51: a ^= read(addr_indy()); SetFlag(Z,a==0); SetFlag(N,a&0x80); break;

    // bit
    case 0x24: { Byte v = read(addr_zp0()); SetFlag(Z,(a&v)==0); SetFlag(N,v&0x80); SetFlag(V,v&0x40);} break;
    case 0x2C: { Byte v = read(addr_abs()); SetFlag(Z,(a&v)==0); SetFlag(N,v&0x80); SetFlag(V,v&0x40);} break;

    // shifts
    case 0x0A: SetFlag(C, a&0x80); a<<=1; SetFlag(Z,a==0); SetFlag(N,a&0x80); break; // ASL A
    case 0x06: { Word ad=addr_zp0(); Byte v=read(ad); SetFlag(C,v&0x80); v<<=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x16: { Word ad=addr_zpx(); Byte v=read(ad); SetFlag(C,v&0x80); v<<=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x0E: { Word ad=addr_abs(); Byte v=read(ad); SetFlag(C,v&0x80); v<<=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x1E: { Word ad=addr_absx(); Byte v=read(ad); SetFlag(C,v&0x80); v<<=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;

    case 0x4A: SetFlag(C, a&1); a>>=1; SetFlag(Z,a==0); SetFlag(N,0); break; // LSR A
    case 0x46: { Word ad=addr_zp0(); Byte v=read(ad); SetFlag(C,v&1); v>>=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,0);} break;
    case 0x56: { Word ad=addr_zpx(); Byte v=read(ad); SetFlag(C,v&1); v>>=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,0);} break;
    case 0x4E: { Word ad=addr_abs(); Byte v=read(ad); SetFlag(C,v&1); v>>=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,0);} break;
    case 0x5E: { Word ad=addr_absx(); Byte v=read(ad); SetFlag(C,v&1); v>>=1; write(ad,v); SetFlag(Z,v==0); SetFlag(N,0);} break;

    // ROL
    case 0x2A: { Byte c=GetFlag(C); SetFlag(C,a&0x80); a=(a<<1)|c; SetFlag(Z,a==0); SetFlag(N,a&0x80);} break;
    case 0x26: { Word ad=addr_zp0(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&0x80); v=(v<<1)|c; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x2E: { Word ad=addr_abs(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&0x80); v=(v<<1)|c; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x3E: { Word ad=addr_absx(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&0x80); v=(v<<1)|c; write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;

    // ROR
    case 0x6A: { Byte c=GetFlag(C); SetFlag(C,a&1); a=(a>>1)|(c?0x80:0); SetFlag(Z,a==0); SetFlag(N,a&0x80);} break;
    case 0x66: { Word ad=addr_zp0(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&1); v=(v>>1)|(c?0x80:0); write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x6E: { Word ad=addr_abs(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&1); v=(v>>1)|(c?0x80:0); write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;
    case 0x7E: { Word ad=addr_absx(); Byte v=read(ad); Byte c=GetFlag(C); SetFlag(C,v&1); v=(v>>1)|(c?0x80:0); write(ad,v); SetFlag(Z,v==0); SetFlag(N,v&0x80);} break;

    // jumps
    case 0x4C: pc = fetchWord(); break;              // JMP abs
    case 0x6C: pc = readPtr(bus, fetchWord()); break; // JMP (indirect) w/ bug
    case 0x20: { Word ret = pc + 1; Word tgt = fetchWord(); push((ret>>8)&0xFF); push(ret&0xFF); pc = tgt; } break; // JSR
    case 0x60: { Byte lo = pop(); Byte hi = pop(); pc = ((hi<<8)|lo) + 1; } break; // RTS
    case 0x40: { status = (pop() & ~B) | U; Byte lo = pop(); Byte hi = pop(); pc = ((hi<<8)|lo); } break; // RTI

    // branches
    case 0x10: branch(!GetFlag(N)); break;
    case 0x30: branch( GetFlag(N)); break;
    case 0x50: branch(!GetFlag(V)); break;
    case 0x70: branch( GetFlag(V)); break;
    case 0x90: branch(!GetFlag(C)); break;
    case 0xB0: branch( GetFlag(C)); break;
    case 0xD0: branch(!GetFlag(Z)); break;
    case 0xF0: branch( GetFlag(Z)); break;

    // flags
    case 0x18: SetFlag(C,false); break;
    case 0x38: SetFlag(C,true);  break;
    case 0x58: SetFlag(I,false); break;
    case 0x78: SetFlag(I,true);  break;
    case 0xB8: SetFlag(V,false); break;
    case 0xD8: SetFlag(D,false); break;
    case 0xF8: SetFlag(D,true);  break;

    case 0xEA: break; // NOP
    case 0x00: break; // BRK (simplified)

    default: break; // unknown ??? ignore for now
    }
}

void CPU::Clock() {
    if (nmi_pending) {
        nmi_pending = false;
        push((pc>>8)&0xFF); push(pc&0xFF);
        push((status & ~B) | U);
        SetFlag(I, true);
        pc = read(0xFFFA) | (read(0xFFFB) << 8);
    } else if (irq_pending && !GetFlag(I)) {
                irq_pending = false;
        push((pc>>8)&0xFF); push(pc&0xFF);
        push((status & ~B) | U);
        SetFlag(I, true);
        pc = read(0xFFFE) | (read(0xFFFF) << 8);
    }

    Byte opcode = fetch();
    execute(opcode);
}
void CPU::serialize(Serializer& s) {
    s.field(a); s.field(x); s.field(y); s.field(stkp); s.field(status);
    s.field(pc);
    s.field(nmi_pending); s.field(irq_pending);
}