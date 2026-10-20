#pragma once
#include "types.h"
#include <string>
#include <vector>

class Bus;
class CPU;
class Serializer;

struct INSTRUCTION {
    std::string name;
    Byte (CPU::*operate)(void) = nullptr;
    Byte (CPU::*addrmode)(void) = nullptr;
    Byte cycles = 0;
};

class CPU {
public:
    CPU();
    ~CPU();

    void ConnectBus(Bus* b) { bus = b; }
    void Reset();
    void IRQ();
    void NMI();
    void Clock();

    Byte a = 0x00, x = 0x00, y = 0x00, stkp = 0xFD, status = 0x24;
    Word pc = 0x0000;

    void serialize(Serializer& s);

private:
    Bus* bus = nullptr;

    Byte read(Word addr);
    void write(Word addr, Byte data);
    Byte fetch();
    Byte fetched = 0x00;
    void SetFlag(Byte f, bool v);
    bool GetFlag(Byte f) const;

    void push(Byte v);
    Byte pop();

    Word temp = 0x0000;
    Word addr_abs = 0x0000;
    Word addr_rel = 0x00;
    Byte opcode = 0x00;
    Byte cycles = 0;
    Word clock_count = 0;

    std::vector<INSTRUCTION> lookup;
    bool nmi_pending = false;
    bool irq_pending = false;

    Byte IMP(); Byte IMM(); Byte ZP0(); Byte ZPX(); Byte ZPY();
    Byte ABS(); Byte ABX(); Byte ABY(); Byte IND(); Byte IZX();
    Byte IZY(); Byte REL();

    Byte ADC(); Byte AND(); Byte ASL(); Byte BCC(); Byte BCS();
    Byte BEQ(); Byte BIT(); Byte BMI(); Byte BNE(); Byte BPL();
    Byte BRK(); Byte BVC(); Byte BVS(); Byte CLC(); Byte CLD();
    Byte CLI(); Byte CLV(); Byte CMP(); Byte CPX(); Byte CPY();
    Byte DEC(); Byte DEX(); Byte DEY(); Byte EOR(); Byte INC();
    Byte INX(); Byte INY(); Byte JMP(); Byte JSR(); Byte LDA();
    Byte LDX(); Byte LDY(); Byte LSR(); Byte NOP(); Byte ORA();
    Byte PHA(); Byte PHP(); Byte PLA(); Byte PLP(); Byte ROL();
    Byte ROR(); Byte RTI(); Byte RTS(); Byte SBC(); Byte SEC();
    Byte SED(); Byte SEI(); Byte STA(); Byte STX(); Byte STY();
    Byte TAX(); Byte TAY(); Byte TSX(); Byte TXA(); Byte TXS();
    Byte TYA(); Byte XXX();
};
