#pragma once
#include "types.h"
#include <functional>
#include <string>

class Bus;

class CPU {
public:
    CPU();
    ~CPU();

    void ConnectBus(Bus* b) { bus = b; }

    Byte a = 0, x = 0, y = 0, stkp = 0xFD, status = 0x24;
    Word pc = 0;

    void Reset();
    void serialize(class Serializer& s);
    void Clock();
    void IRQ();
    void NMI();

    bool complete = false;

private:
    Bus* bus = nullptr;

    Byte read(Word addr);
    void write(Word addr, Byte data);
    Byte fetch();
    Word fetchWord();

    Word readWord(Word addr);

    enum FLAGS { C=1, Z=2, I=4, D=8, B=16, U=32, V=64, N=128 };
    void SetFlag(FLAGS f, bool v);
    bool GetFlag(FLAGS f) const;

    void push(Byte v);
    Byte pop();

    Word addr_imm(); Word addr_zp0(); Word addr_zpx(); Word addr_zpy();
    Word addr_abs(); Word addr_absx(); Word addr_absy();
    Word addr_indx(); Word addr_indy();

    void ADC(Byte v); void SBC(Byte v); void CMP(Byte r, Byte v);
    void branch(bool cond);

    void execute(Byte opcode);

    bool nmi_pending = false;
    bool irq_pending = false;
};
