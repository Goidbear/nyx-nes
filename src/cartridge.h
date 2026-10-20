#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <memory>

class Cartridge {
public:
    Cartridge(const std::string& path);
    bool ImageValid() const { return valid; }

    enum MIRROR { HORIZONTAL, VERTICAL, ONESCREEN_LO, ONESCREEN_HI } mirror = HORIZONTAL;

    std::vector<Byte> PRG;
    std::vector<Byte> CHR;
    std::vector<Byte> prgRam;
    Byte mapperID = 0;
    bool valid = false;
    bool isPAL = false;

    // MMC1
    Byte controlReg = 0x0C;
    Byte chrBank0 = 0, chrBank1 = 0, prgBank = 0;
    Byte shiftReg = 0;
    int shiftCount = 0;

    // GNROM (66)
    Byte gnromByte = 0;

    // MMC3
    Byte mmc3BankSelect = 0;
    Byte mmc3BankRegs[8] = {0,0,0,0,0,0,0,0};
    Byte mmc3IRQLatch = 0;
    Byte mmc3IRQCounter = 0;
    bool mmc3IRQReload = false;
    bool mmc3IRQEnabled = false;
    bool mmc3IRQPending = false;

    // IRQ wire exposed to bus
    bool irqPending = false;
    int bankWriteCount = 0;

    bool cpuRead(Word addr, Byte& data);
    bool cpuWrite(Word addr, Byte data);
    bool ppuRead(Word addr, Byte& data);
    bool ppuWrite(Word addr, Byte data);
    void clockIRQCounter();
    void serialize(class Serializer& s);
    void saveBatteryRam(const std::string& romPath);
    void loadBatteryRam(const std::string& romPath);

private:
    Byte nPRGBanks = 0;
    Byte nCHRBanks = 0;

    bool cpuReadMapper0(Word addr, Byte& data);
    bool cpuReadMapper1(Word addr, Byte& data);
    bool cpuWriteMapper1(Word addr, Byte data);
    bool ppuReadMapper0(Word addr, Byte& data);
    bool ppuReadMapper1(Word addr, Byte& data);
    bool ppuWriteMapper1(Word addr, Byte data);

    bool cpuReadMapper2(Word addr, Byte& data);
    bool cpuWriteMapper2(Word addr, Byte data);
    bool ppuReadMapper2(Word addr, Byte& data);
    bool ppuWriteMapper2(Word addr, Byte data);

    bool cpuReadMapper3(Word addr, Byte& data);
    bool cpuWriteMapper3(Word addr, Byte data);
    bool ppuReadMapper3(Word addr, Byte& data);

    bool cpuReadMapper4(Word addr, Byte& data);
    bool cpuWriteMapper4(Word addr, Byte data);
    bool ppuReadMapper4(Word addr, Byte& data);
    bool ppuWriteMapper4(Word addr, Byte data);

    bool cpuReadMapper7(Word addr, Byte& data);
    bool cpuWriteMapper7(Word addr, Byte data);
    bool ppuReadMapper7(Word addr, Byte& data);

    bool cpuReadMapper66(Word addr, Byte& data);
    bool cpuWriteMapper66(Word addr, Byte data);
    bool ppuReadMapper66(Word addr, Byte& data);
};
