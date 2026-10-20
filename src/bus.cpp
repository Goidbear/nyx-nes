#include <cstdio>
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include <SDL2/SDL.h>
#include "savestate.h"

Bus::Bus() {
    cpu = new CPU();
    cpu->ConnectBus(this);
    ppu = new PPU();
    ppu->ConnectCartridge(nullptr);
    apu = new APU();
}

Bus::~Bus() { delete cpu; delete ppu; delete apu; }

void Bus::insertCartridge(std::shared_ptr<Cartridge> c) {
    cart = c;
    ppu->ConnectCartridge(c.get());
    palMode = c->isPAL;
    ppu->setPAL(palMode);
    if (apu) apu->setPAL(palMode);
}

Byte Bus::cpuRead(Word addr) {
    Byte data = 0;
    if (cart && cart->cpuRead(addr, data)) { }
    else if (addr >= 0x8000 && cart && !cart->PRG.empty()) {
        // Safety net: unsupported mapper — fall back to last PRG bank so the
        // CPU never ends up executing RAM. Games will still be broken but at
        // least we boot to a black screen instead of hanging at $0000.
        Word mask = (cart->PRG.size() >= 0x8000) ? 0x7FFF : (Word)(cart->PRG.size() - 1);
        data = cart->PRG[cart->PRG.size() - 0x8000 + (addr & 0x7FFF) & (cart->PRG.size() - 1)];
    }
    else if (addr <= 0x1FFF) data = cpuRam[addr & 0x07FF];
    else if (addr <= 0x3FFF) data = ppu->cpuRead(addr & 0x0007);
    else if (addr == 0x4015) data = apu->cpuRead(addr);
    else if (addr == 0x4016) {
        if (controller_strobe) data = controller1 & 1;
        else { data = controller1_shift & 1; controller1_shift >>= 1; }
    }
    else if (addr == 0x4017) {
        if (zapperEnabled) {
            // Zapper present on port 2. Bits: 0,1,2,5,6,7 = 1 (open bus)
            // bit 3 = light (1 = dark, 0 = bright), bit 4 = trigger (1 = released, 0 = pulled)
            Byte result = 0xE7;

            bool lightDetected = false;
            int zx = zapperX; int zy = zapperY;
            if (zx < 0) zx = 0; if (zx > 255) zx = 255;
            if (zy < 0) zy = 0; if (zy > 239) zy = 239;

            if (zapperOnScreen) {
                uint32_t c = ppu->frame[zy * 256 + zx];
                Byte r = (c >> 16) & 0xFF;
                Byte g = (c >>  8) & 0xFF;
                Byte b = (c      ) & 0xFF;
                int lum = (r * 299 + g * 587 + b * 114) / 1000;
                lightDetected = lum > 100;  // relaxed from 128
            }

            if (!lightDetected) result |= 0x08;
            if (!zapperTrigger) result |= 0x10;
            data = result;

            static int zpLog = 0;
            if (zpLog < 200) {
                fprintf(stderr, "ZP read %02X: trig=%d light=%d x=%d y=%d scan=%d mask=%02X\n",
                        data, (int)zapperTrigger, (int)lightDetected,
                        zx, zy, ppu->GetScanline(), ppu ? ppu->GetMask() : 0);
                zpLog++;
            }
        } else {
            if (controller_strobe) data = controller2 & 1;
            else { data = controller2_shift & 1; controller2_shift >>= 1; }
        }
    }
    return data;
}

void Bus::cpuWrite(Word addr, Byte data) {
    if (cart && cart->cpuWrite(addr, data)) { }
    else if (addr <= 0x1FFF) {
        cpuRam[addr & 0x07FF] = data;
    }
    else if (addr <= 0x3FFF) ppu->cpuWrite(addr & 0x0007, data);
    else if (addr == 0x4014) {
        Word base = (Word)data << 8;
        for (int i = 0; i < 256; i++)
            ppu->cpuWrite(0x0004, cpuRead(base + i));
    }
    else if (addr == 0x4016) {
        controller_strobe = data & 1;
        if (controller_strobe) {
            controller1_shift = controller1;
            controller2_shift = controller2;
        }
    }
    else if (addr >= 0x4000 && addr <= 0x4017) apu->cpuWrite(addr, data);
}

Byte Bus::ppuRead(Word addr, bool readOnly) {
    Byte data = 0;
    if (cart && cart->ppuRead(addr, data)) {}
    else data = ppu->ppuRead(addr, readOnly);
    return data;
}

void Bus::ppuWrite(Word addr, Byte data) {
    if (cart && cart->ppuWrite(addr, data)) {}
    else ppu->ppuWrite(addr, data);
}

void Bus::clock() {
    ppu->clock();
    if (ppu->nmi) { ppu->nmi = false; cpu->NMI(); }
        auto runCPU = [&]() {
        if (cart && cart->irqPending) {
                        cart->irqPending = false;
            cpu->IRQ();
        }
        if (cart) cart->irqPending = false; // always clear
        cpu->Clock();
        apu->clock();
    };
    if (palMode) {
        palPhase = (palPhase + 1) & 15;
        if (palPhase < 5) runCPU();
    } else {
        if (systemClockCounter % 3 == 0) runCPU();
    }
    systemClockCounter++;
}
void Bus::serialize(Serializer& s) {
    s.array(cpuRam);
    s.field(controller1);
    s.field(controller2);
    s.field(controller1_shift);
    s.field(controller2_shift);
    s.field(controller_strobe);
    s.field(palMode);
    s.field(palPhase);
    s.field(systemClockCounter);
    if (cpu) cpu->serialize(s);
    if (ppu) ppu->serialize(s);
    if (apu) apu->serialize(s);
    if (cart) cart->serialize(s);
}