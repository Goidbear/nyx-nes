#include "cpu.h"
#include "bus.h"
#include "savestate.h"

CPU::CPU() {
    lookup.resize(256);

    auto set = [&](Byte op, std::string name, Byte (CPU::*opFn)(void), Byte (CPU::*amFn)(void), Byte cyc) {
        lookup[op].name = name;
        lookup[op].operate = opFn;
        lookup[op].addrmode = amFn;
        lookup[op].cycles = cyc;
    };

    for (int i = 0; i < 256; i++) set(i, "???", &CPU::XXX, &CPU::IMP, 2);

    set(0x69, "ADC", &CPU::ADC, &CPU::IMM, 2); set(0x65, "ADC", &CPU::ADC, &CPU::ZP0, 3); set(0x75, "ADC", &CPU::ADC, &CPU::ZPX, 4);
    set(0x6D, "ADC", &CPU::ADC, &CPU::ABS, 4); set(0x7D, "ADC", &CPU::ADC, &CPU::ABX, 4); set(0x79, "ADC", &CPU::ADC, &CPU::ABY, 4);
    set(0x61, "ADC", &CPU::ADC, &CPU::IZX, 6); set(0x71, "ADC", &CPU::ADC, &CPU::IZY, 5);

    set(0x29, "AND", &CPU::AND, &CPU::IMM, 2); set(0x25, "AND", &CPU::AND, &CPU::ZP0, 3); set(0x35, "AND", &CPU::AND, &CPU::ZPX, 4);
    set(0x2D, "AND", &CPU::AND, &CPU::ABS, 4); set(0x3D, "AND", &CPU::AND, &CPU::ABX, 4); set(0x39, "AND", &CPU::AND, &CPU::ABY, 4);
    set(0x21, "AND", &CPU::AND, &CPU::IZX, 6); set(0x31, "AND", &CPU::AND, &CPU::IZY, 5);

    set(0x0A, "ASL", &CPU::ASL, &CPU::IMP, 2); set(0x06, "ASL", &CPU::ASL, &CPU::ZP0, 5); set(0x16, "ASL", &CPU::ASL, &CPU::ZPX, 6);
    set(0x0E, "ASL", &CPU::ASL, &CPU::ABS, 6); set(0x1E, "ASL", &CPU::ASL, &CPU::ABX, 7);

    set(0x90, "BCC", &CPU::BCC, &CPU::REL, 2); set(0xB0, "BCS", &CPU::BCS, &CPU::REL, 2); set(0xF0, "BEQ", &CPU::BEQ, &CPU::REL, 2);
    set(0x30, "BMI", &CPU::BMI, &CPU::REL, 2); set(0xD0, "BNE", &CPU::BNE, &CPU::REL, 2); set(0x10, "BPL", &CPU::BPL, &CPU::REL, 2);
    set(0x50, "BVC", &CPU::BVC, &CPU::REL, 2); set(0x70, "BVS", &CPU::BVS, &CPU::REL, 2);

    set(0x24, "BIT", &CPU::BIT, &CPU::ZP0, 3); set(0x2C, "BIT", &CPU::BIT, &CPU::ABS, 4);
    set(0x00, "BRK", &CPU::BRK, &CPU::IMM, 7);

    set(0x18, "CLC", &CPU::CLC, &CPU::IMP, 2); set(0xD8, "CLD", &CPU::CLD, &CPU::IMP, 2); set(0x58, "CLI", &CPU::CLI, &CPU::IMP, 2); set(0xB8, "CLV", &CPU::CLV, &CPU::IMP, 2);

    set(0xC9, "CMP", &CPU::CMP, &CPU::IMM, 2); set(0xC5, "CMP", &CPU::CMP, &CPU::ZP0, 3); set(0xD5, "CMP", &CPU::CMP, &CPU::ZPX, 4);
    set(0xCD, "CMP", &CPU::CMP, &CPU::ABS, 4); set(0xDD, "CMP", &CPU::CMP, &CPU::ABX, 4); set(0xD9, "CMP", &CPU::CMP, &CPU::ABY, 4);
    set(0xC1, "CMP", &CPU::CMP, &CPU::IZX, 6); set(0xD1, "CMP", &CPU::CMP, &CPU::IZY, 5);

    set(0xE0, "CPX", &CPU::CPX, &CPU::IMM, 2); set(0xE4, "CPX", &CPU::CPX, &CPU::ZP0, 3); set(0xEC, "CPX", &CPU::CPX, &CPU::ABS, 4);
    set(0xC0, "CPY", &CPU::CPY, &CPU::IMM, 2); set(0xC4, "CPY", &CPU::CPY, &CPU::ZP0, 3); set(0xCC, "CPY", &CPU::CPY, &CPU::ABS, 4);

    set(0xC6, "DEC", &CPU::DEC, &CPU::ZP0, 5); set(0xD6, "DEC", &CPU::DEC, &CPU::ZPX, 6); set(0xCE, "DEC", &CPU::DEC, &CPU::ABS, 6); set(0xDE, "DEC", &CPU::DEC, &CPU::ABX, 7);
    set(0xCA, "DEX", &CPU::DEX, &CPU::IMP, 2); set(0x88, "DEY", &CPU::DEY, &CPU::IMP, 2);

    set(0x49, "EOR", &CPU::EOR, &CPU::IMM, 2); set(0x45, "EOR", &CPU::EOR, &CPU::ZP0, 3); set(0x55, "EOR", &CPU::EOR, &CPU::ZPX, 4);
    set(0x4D, "EOR", &CPU::EOR, &CPU::ABS, 4); set(0x5D, "EOR", &CPU::EOR, &CPU::ABX, 4); set(0x59, "EOR", &CPU::EOR, &CPU::ABY, 4);
    set(0x41, "EOR", &CPU::EOR, &CPU::IZX, 6); set(0x51, "EOR", &CPU::EOR, &CPU::IZY, 5);

    set(0xE6, "INC", &CPU::INC, &CPU::ZP0, 5); set(0xF6, "INC", &CPU::INC, &CPU::ZPX, 6); set(0xEE, "INC", &CPU::INC, &CPU::ABS, 6); set(0xFE, "INC", &CPU::INC, &CPU::ABX, 7);
    set(0xE8, "INX", &CPU::INX, &CPU::IMP, 2); set(0xC8, "INY", &CPU::INY, &CPU::IMP, 2);

    set(0x4C, "JMP", &CPU::JMP, &CPU::ABS, 3); set(0x6C, "JMP", &CPU::JMP, &CPU::IND, 5);
    set(0x20, "JSR", &CPU::JSR, &CPU::ABS, 6);

    set(0xA9, "LDA", &CPU::LDA, &CPU::IMM, 2); set(0xA5, "LDA", &CPU::LDA, &CPU::ZP0, 3); set(0xB5, "LDA", &CPU::LDA, &CPU::ZPX, 4);
    set(0xAD, "LDA", &CPU::LDA, &CPU::ABS, 4); set(0xBD, "LDA", &CPU::LDA, &CPU::ABX, 4); set(0xB9, "LDA", &CPU::LDA, &CPU::ABY, 4);
    set(0xA1, "LDA", &CPU::LDA, &CPU::IZX, 6); set(0xB1, "LDA", &CPU::LDA, &CPU::IZY, 5);

    set(0xA2, "LDX", &CPU::LDX, &CPU::IMM, 2); set(0xA6, "LDX", &CPU::LDX, &CPU::ZP0, 3); set(0xB6, "LDX", &CPU::LDX, &CPU::ZPY, 4);
    set(0xAE, "LDX", &CPU::LDX, &CPU::ABS, 4); set(0xBE, "LDX", &CPU::LDX, &CPU::ABY, 4);

    set(0xA0, "LDY", &CPU::LDY, &CPU::IMM, 2); set(0xA4, "LDY", &CPU::LDY, &CPU::ZP0, 3); set(0xB4, "LDY", &CPU::LDY, &CPU::ZPX, 4);
    set(0xAC, "LDY", &CPU::LDY, &CPU::ABS, 4); set(0xBC, "LDY", &CPU::LDY, &CPU::ABX, 4);

    set(0x4A, "LSR", &CPU::LSR, &CPU::IMP, 2); set(0x46, "LSR", &CPU::LSR, &CPU::ZP0, 5); set(0x56, "LSR", &CPU::LSR, &CPU::ZPX, 6);
    set(0x4E, "LSR", &CPU::LSR, &CPU::ABS, 6); set(0x5E, "LSR", &CPU::LSR, &CPU::ABX, 7);

    set(0xEA, "NOP", &CPU::NOP, &CPU::IMP, 2);

    set(0x09, "ORA", &CPU::ORA, &CPU::IMM, 2); set(0x05, "ORA", &CPU::ORA, &CPU::ZP0, 3); set(0x15, "ORA", &CPU::ORA, &CPU::ZPX, 4);
    set(0x0D, "ORA", &CPU::ORA, &CPU::ABS, 4); set(0x1D, "ORA", &CPU::ORA, &CPU::ABX, 4); set(0x19, "ORA", &CPU::ORA, &CPU::ABY, 4);
    set(0x01, "ORA", &CPU::ORA, &CPU::IZX, 6); set(0x11, "ORA", &CPU::ORA, &CPU::IZY, 5);

    set(0x48, "PHA", &CPU::PHA, &CPU::IMP, 3); set(0x08, "PHP", &CPU::PHP, &CPU::IMP, 3);
    set(0x68, "PLA", &CPU::PLA, &CPU::IMP, 4); set(0x28, "PLP", &CPU::PLP, &CPU::IMP, 4);

    set(0x2A, "ROL", &CPU::ROL, &CPU::IMP, 2); set(0x26, "ROL", &CPU::ROL, &CPU::ZP0, 5); set(0x36, "ROL", &CPU::ROL, &CPU::ZPX, 6);
    set(0x2E, "ROL", &CPU::ROL, &CPU::ABS, 6); set(0x3E, "ROL", &CPU::ROL, &CPU::ABX, 7);

    set(0x6A, "ROR", &CPU::ROR, &CPU::IMP, 2); set(0x66, "ROR", &CPU::ROR, &CPU::ZP0, 5); set(0x76, "ROR", &CPU::ROR, &CPU::ZPX, 6);
    set(0x6E, "ROR", &CPU::ROR, &CPU::ABS, 6); set(0x7E, "ROR", &CPU::ROR, &CPU::ABX, 7);

    set(0x40, "RTI", &CPU::RTI, &CPU::IMP, 6); set(0x60, "RTS", &CPU::RTS, &CPU::IMP, 6);

    set(0xE9, "SBC", &CPU::SBC, &CPU::IMM, 2); set(0xE5, "SBC", &CPU::SBC, &CPU::ZP0, 3); set(0xF5, "SBC", &CPU::SBC, &CPU::ZPX, 4);
    set(0xED, "SBC", &CPU::SBC, &CPU::ABS, 4); set(0xFD, "SBC", &CPU::SBC, &CPU::ABX, 4); set(0xF9, "SBC", &CPU::SBC, &CPU::ABY, 4);
    set(0xE1, "SBC", &CPU::SBC, &CPU::IZX, 6); set(0xF1, "SBC", &CPU::SBC, &CPU::IZY, 5);

    set(0x38, "SEC", &CPU::SEC, &CPU::IMP, 2); set(0xF8, "SED", &CPU::SED, &CPU::IMP, 2); set(0x78, "SEI", &CPU::SEI, &CPU::IMP, 2);

    set(0x85, "STA", &CPU::STA, &CPU::ZP0, 3); set(0x95, "STA", &CPU::STA, &CPU::ZPX, 4); set(0x8D, "STA", &CPU::STA, &CPU::ABS, 4);
    set(0x9D, "STA", &CPU::STA, &CPU::ABX, 5); set(0x99, "STA", &CPU::STA, &CPU::ABY, 5); set(0x81, "STA", &CPU::STA, &CPU::IZX, 6); set(0x91, "STA", &CPU::STA, &CPU::IZY, 6);

    set(0x86, "STX", &CPU::STX, &CPU::ZP0, 3); set(0x96, "STX", &CPU::STX, &CPU::ZPY, 4); set(0x8E, "STX", &CPU::STX, &CPU::ABS, 4);
    set(0x84, "STY", &CPU::STY, &CPU::ZP0, 3); set(0x94, "STY", &CPU::STY, &CPU::ZPX, 4); set(0x8C, "STY", &CPU::STY, &CPU::ABS, 4);

    set(0xAA, "TAX", &CPU::TAX, &CPU::IMP, 2); set(0xA8, "TAY", &CPU::TAY, &CPU::IMP, 2);
    set(0xBA, "TSX", &CPU::TSX, &CPU::IMP, 2); set(0x8A, "TXA", &CPU::TXA, &CPU::IMP, 2);
    set(0x9A, "TXS", &CPU::TXS, &CPU::IMP, 2); set(0x98, "TYA", &CPU::TYA, &CPU::IMP, 2);
}

CPU::~CPU() {}
Byte CPU::read(Word addr) { return bus->cpuRead(addr); }
void CPU::write(Word addr, Byte data) { bus->cpuWrite(addr, data); }
Byte CPU::fetch() {
    if (lookup[opcode].addrmode != &CPU::IMP) fetched = read(addr_abs);
    return fetched;
}
void CPU::SetFlag(Byte f, bool v) { if (v) status |= f; else status &= ~f; }
bool CPU::GetFlag(Byte f) const { return (status & f) > 0; }
void CPU::push(Byte v) { write(0x0100 + stkp, v); stkp--; }

Byte CPU::pop() {
    stkp++;
    return read(0x0100 + stkp);
}

void CPU::Reset() {
    a = 0; x = 0; y = 0;
    stkp = 0xFD; status = 0x24;
    pc = read(0xFFFC) | (read(0xFFFD) << 8);
    cycles = 8; clock_count = 0;
}

void CPU::IRQ() {
    if (GetFlag(0x04)) return;
    irq_pending = true;
}
void CPU::NMI() { nmi_pending = true; }

void CPU::Clock() {
    if (cycles == 0) {
        if (nmi_pending) {
            nmi_pending = false;
            push((pc >> 8) & 0xFF); push(pc & 0xFF);
            SetFlag(0x10, false); push(status);
            SetFlag(0x04, true);
            pc = read(0xFFFA) | (read(0xFFFB) << 8);
            cycles = 8;
            return;
        }
        if (irq_pending) {
            irq_pending = false;
            push((pc >> 8) & 0xFF); push(pc & 0xFF);
            SetFlag(0x10, false); push(status);
            SetFlag(0x04, true);
            pc = read(0xFFFE) | (read(0xFFFF) << 8);
            cycles = 7;
            return;
        }

        opcode = read(pc);
        SetFlag(0x10, true);
        pc++;
        cycles = lookup[opcode].cycles;
        
        Byte extra1 = (lookup[opcode].addrmode ? (this->*lookup[opcode].addrmode)() : 0);
        Byte extra2 = (lookup[opcode].operate ? (this->*lookup[opcode].operate)() : 0);
        
        cycles += (extra1 & extra2);
        SetFlag(0x10, false);
    }
    clock_count++;
    cycles--;
}

Byte CPU::IMP() { return 0; }
Byte CPU::IMM() { addr_abs = pc++; return 0; }
Byte CPU::ZP0() { addr_abs = read(pc++); return 0; }
Byte CPU::ZPX() { addr_abs = (read(pc++) + x) & 0x00FF; return 0; }
Byte CPU::ZPY() { addr_abs = (read(pc++) + y) & 0x00FF; return 0; }
Byte CPU::ABS() { Word lo = read(pc++); Word hi = read(pc++); addr_abs = (hi << 8) | lo; return 0; }
Byte CPU::ABX() { Word lo = read(pc++); Word hi = read(pc++); addr_abs = ((hi << 8) | lo) + x; if ((addr_abs & 0xFF00) != (hi << 8)) return 1; return 0; }
Byte CPU::ABY() { Word lo = read(pc++); Word hi = read(pc++); addr_abs = ((hi << 8) | lo) + y; if ((addr_abs & 0xFF00) != (hi << 8)) return 1; return 0; }
Byte CPU::IND() {
    Word lo = read(pc++); Word hi = read(pc++); Word ptr = (hi << 8) | lo;
    if (lo == 0xFF) addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr);
    else addr_abs = (read(ptr + 1) << 8) | read(ptr);
    return 0;
}
Byte CPU::IZX() {
    Word t = read(pc++);
    Word lo = read((Word)(t + x) & 0x00FF); Word hi = read((Word)(t + x + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo; return 0;
}
Byte CPU::IZY() {
    Word t = read(pc++);
    Word lo = read(t & 0x00FF); Word hi = read((t + 1) & 0x00FF);
    addr_abs = ((hi << 8) | lo) + y; if ((addr_abs & 0xFF00) != (hi << 8)) return 1; return 0;
}
Byte CPU::REL() {
    addr_rel = read(pc++);
    if (addr_rel & 0x80) addr_rel |= 0xFF00;
    return 0;
}

Byte CPU::ADC() {
    fetch(); Word sum = a + fetched + (GetFlag(0x01) ? 1 : 0);
    SetFlag(0x01, sum > 255); SetFlag(0x40, (~(a ^ fetched) & (a ^ sum)) & 0x80);
    a = sum & 0xFF; SetFlag(0x80, a & 0x80); SetFlag(0x02, a == 0); return 1;
}
Byte CPU::AND() { fetch(); a &= fetched; SetFlag(0x80, a & 0x80); SetFlag(0x02, a == 0); return 1; }
Byte CPU::ASL() {
    if (lookup[opcode].addrmode == &CPU::IMP) { Byte t = a; SetFlag(0x01, t & 0x80); a <<= 1; SetFlag(0x80, a & 0x80); SetFlag(0x02, a == 0); }
    else { fetch(); Byte t = fetched; SetFlag(0x01, t & 0x80); t <<= 1; write(addr_abs, t); SetFlag(0x80, t & 0x80); SetFlag(0x02, t == 0); }
    return 0;
}
Byte CPU::BCC() { if (!GetFlag(0x01)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BCS() { if (GetFlag(0x01)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BEQ() { if (GetFlag(0x02)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BMI() { if (GetFlag(0x80)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BNE() { if (!GetFlag(0x02)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BPL() { if (!GetFlag(0x80)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BVC() { if (!GetFlag(0x40)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BVS() { if (GetFlag(0x40)) { cycles++; addr_abs = pc + addr_rel; if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++; pc = addr_abs; } return 0; }
Byte CPU::BIT() { fetch(); temp = a & fetched; SetFlag(0x02, temp == 0); SetFlag(0x80, fetched & 0x80); SetFlag(0x40, fetched & 0x40); return 0; }
Byte CPU::BRK() { pc++; push((pc >> 8) & 0xFF); push(pc & 0xFF); push(status | 0x10); SetFlag(0x04, true); pc = read(0xFFFE) | (read(0xFFFF) << 8); return 0; }
Byte CPU::CLC() { SetFlag(0x01, false); return 0; }
Byte CPU::CLD() { SetFlag(0x08, false); return 0; }
Byte CPU::CLI() { SetFlag(0x04, false); return 0; }
Byte CPU::CLV() { SetFlag(0x40, false); return 0; }
Byte CPU::CMP() { fetch(); temp = a - fetched; SetFlag(0x01, a >= fetched); SetFlag(0x02, temp == 0); SetFlag(0x80, temp & 0x80); return 1; }
Byte CPU::CPX() { fetch(); temp = x - fetched; SetFlag(0x01, x >= fetched); SetFlag(0x02, temp == 0); SetFlag(0x80, temp & 0x80); return 0; }
Byte CPU::CPY() { fetch(); temp = y - fetched; SetFlag(0x01, y >= fetched); SetFlag(0x02, temp == 0); SetFlag(0x80, temp & 0x80); return 0; }
Byte CPU::DEC() { fetch(); temp = fetched - 1; write(addr_abs, temp & 0xFF); SetFlag(0x02, temp == 0); SetFlag(0x80, temp & 0x80); return 0; }
Byte CPU::DEX() { x--; SetFlag(0x02, x == 0); SetFlag(0x80, x & 0x80); return 0; }
Byte CPU::DEY() { y--; SetFlag(0x02, y == 0); SetFlag(0x80, y & 0x80); return 0; }
Byte CPU::EOR() { fetch(); a ^= fetched; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 1; }
Byte CPU::INC() { fetch(); temp = fetched + 1; write(addr_abs, temp & 0xFF); SetFlag(0x02, temp == 0); SetFlag(0x80, temp & 0x80); return 0; }
Byte CPU::INX() { x++; SetFlag(0x02, x == 0); SetFlag(0x80, x & 0x80); return 0; }
Byte CPU::INY() { y++; SetFlag(0x02, y == 0); SetFlag(0x80, y & 0x80); return 0; }
Byte CPU::JMP() { pc = addr_abs; return 0; }
Byte CPU::JSR() { pc--; push((pc >> 8) & 0xFF); push(pc & 0xFF); pc = addr_abs; return 0; }
Byte CPU::LDA() { fetch(); a = fetched; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 1; }
Byte CPU::LDX() { fetch(); x = fetched; SetFlag(0x02, x == 0); SetFlag(0x80, x & 0x80); return 1; }
Byte CPU::LDY() { fetch(); y = fetched; SetFlag(0x02, y == 0); SetFlag(0x80, y & 0x80); return 1; }
Byte CPU::LSR() {
    if (lookup[opcode].addrmode == &CPU::IMP) { SetFlag(0x01, a & 0x01); a >>= 1; SetFlag(0x02, a == 0); SetFlag(0x80, 0); }
    else { fetch(); Byte t = fetched; SetFlag(0x01, t & 0x01); t >>= 1; write(addr_abs, t); SetFlag(0x02, t == 0); SetFlag(0x80, 0); }
    return 0;
}
Byte CPU::NOP() { return 0; }
Byte CPU::ORA() { fetch(); a |= fetched; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 1; }
Byte CPU::PHA() { push(a); return 0; }
Byte CPU::PHP() { push(status | 0x10 | 0x20); return 0; }
Byte CPU::PLA() { a = pop(); SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 0; }
Byte CPU::PLP() { status = pop(); SetFlag(0x20, true); SetFlag(0x10, false); return 0; }
Byte CPU::ROL() {
    if (lookup[opcode].addrmode == &CPU::IMP) { Byte t = a; Byte c = GetFlag(0x01); SetFlag(0x01, t & 0x80); a = (t << 1) | c; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); }
    else { fetch(); Byte t = fetched; Byte c = GetFlag(0x01); SetFlag(0x01, t & 0x80); t = (t << 1) | c; write(addr_abs, t); SetFlag(0x02, t == 0); SetFlag(0x80, t & 0x80); }
    return 0;
}
Byte CPU::ROR() {
    if (lookup[opcode].addrmode == &CPU::IMP) { Byte t = a; Byte c = GetFlag(0x01); SetFlag(0x01, t & 0x01); a = (t >> 1) | (c << 7); SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); }
    else { fetch(); Byte t = fetched; Byte c = GetFlag(0x01); SetFlag(0x01, t & 0x01); t = (t >> 1) | (c << 7); write(addr_abs, t); SetFlag(0x02, t == 0); SetFlag(0x80, t & 0x80); }
    return 0;
}
Byte CPU::RTI() { status = pop(); SetFlag(0x20, true); SetFlag(0x10, false); Byte lo = pop(); Byte hi = pop(); pc = (hi << 8) | lo; return 0; }
Byte CPU::RTS() { Byte lo = pop(); Byte hi = pop(); pc = (hi << 8) | lo; pc++; return 0; }
Byte CPU::SBC() { fetch(); Word v = ((Word)fetched) ^ 0x00FF; Word sum = a + v + (GetFlag(0x01) ? 1 : 0); SetFlag(0x01, sum > 255); SetFlag(0x40, (~(a ^ v) & (a ^ sum)) & 0x80); a = sum & 0xFF; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 1; }
Byte CPU::SEC() { SetFlag(0x01, true); return 0; }
Byte CPU::SED() { SetFlag(0x08, true); return 0; }
Byte CPU::SEI() { SetFlag(0x04, true); return 0; }
Byte CPU::STA() { write(addr_abs, a); return 0; }
Byte CPU::STX() { write(addr_abs, x); return 0; }
Byte CPU::STY() { write(addr_abs, y); return 0; }
Byte CPU::TAX() { x = a; SetFlag(0x02, x == 0); SetFlag(0x80, x & 0x80); return 0; }
Byte CPU::TAY() { y = a; SetFlag(0x02, y == 0); SetFlag(0x80, y & 0x80); return 0; }
Byte CPU::TSX() { x = stkp; SetFlag(0x02, x == 0); SetFlag(0x80, x & 0x80); return 0; }
Byte CPU::TXA() { a = x; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 0; }
Byte CPU::TXS() { stkp = x; return 0; }
Byte CPU::TYA() { a = y; SetFlag(0x02, a == 0); SetFlag(0x80, a & 0x80); return 0; }
Byte CPU::XXX() { return 0; }

void CPU::serialize(Serializer& s) {
    s.field(a); s.field(x); s.field(y); s.field(stkp); s.field(status); s.field(pc);
    s.field(fetched); s.field(temp); s.field(addr_abs); s.field(addr_rel);
    s.field(opcode); s.field(cycles); s.field(clock_count);
    s.field(nmi_pending); s.field(irq_pending);
}
