#pragma once
#include "types.h"
#include <memory>
#include <array>

class CPU;
class PPU;
class APU;
class Cartridge;

class Bus {
public:
    Bus();
    ~Bus();

    void insertCartridge(std::shared_ptr<Cartridge> c);
    Byte cpuRead(Word addr);
    void cpuWrite(Word addr, Byte data);
    Byte ppuRead(Word addr, bool readOnly = false);
    void ppuWrite(Word addr, Byte data);
    void clock();
    void serialize(class Serializer& s);

    CPU* cpu = nullptr;
    PPU* ppu = nullptr;
    APU* apu = nullptr;
    std::shared_ptr<Cartridge> cart;
    std::array<Byte, 2048> cpuRam{};

    Byte controller1 = 0;
    Byte controller2 = 0;
    Byte controller1_shift = 0;
    Byte controller2_shift = 0;
    bool controller_strobe = false;

    // Zapper (light gun) ? plugged into port 2 when enabled
    bool zapperEnabled = false;
    bool zapperTrigger = false;
    int zapperX = 128;
    int zapperY = 120;
    bool zapperOnScreen = true;
    int zapperClickPulse = 0;
    int zapperLightHoldFrames = 0;

    bool palMode = false;
    int palPhase = 0;
    int systemClockCounter = 0;
};
