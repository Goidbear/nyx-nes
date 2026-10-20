#include <cstdio>
#include <fstream>
#include "cartridge.h"
#include "savestate.h"

Cartridge::Cartridge(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;

    Byte header[16];
    f.read(reinterpret_cast<char*>(header), 16);
    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A)
        return;

    nPRGBanks = header[4];
    nCHRBanks = header[5];
    mapperID  = (header[6] >> 4) | (header[7] & 0xF0);
    mirror = (header[6] & 1) ? VERTICAL : HORIZONTAL;
    isPAL = (header[9] & 0x01) != 0;

    PRG.resize(nPRGBanks * 16384);
    f.read(reinterpret_cast<char*>(PRG.data()), PRG.size());

    if (nCHRBanks == 0) {
        CHR.resize(8192);
    } else {
        CHR.resize(nCHRBanks * 8192);
        f.read(reinterpret_cast<char*>(CHR.data()), CHR.size());
    }

    if (mapperID == 1) {
        prgRam.resize(8192, 0);
        controlReg = 0x0C;
    }
    if (mapperID == 4) {
        prgRam.resize(8192, 0);
        mmc3BankRegs[6] = 0;
        mmc3BankRegs[7] = 1;
        // MMC3 games often have wrong iNES mirroring; SMB3 and most others expect VERTICAL
        mirror = VERTICAL;
    }

    valid = true;
}

bool Cartridge::cpuRead(Word addr, Byte& data) {
    if (addr >= 0x6000 && addr <= 0x7FFF && !prgRam.empty()) {
        data = prgRam[addr - 0x6000];
        return true;
    }
    if (addr < 0x8000) return false;
    switch (mapperID) {
    case 0:  return cpuReadMapper0(addr, data);
    case 1:  return cpuReadMapper1(addr, data);
    case 2:  return cpuReadMapper2(addr, data);
    case 3:  return cpuReadMapper3(addr, data);
    case 4:  return cpuReadMapper4(addr, data);
    case 7:  return cpuReadMapper7(addr, data);
    case 66: return cpuReadMapper66(addr, data);
    }
    return false;
}

bool Cartridge::cpuWrite(Word addr, Byte data) {
    if (addr >= 0x6000 && addr <= 0x7FFF && !prgRam.empty()) {
        prgRam[addr - 0x6000] = data;
        return true;
    }
    if (addr < 0x8000) return false;
    switch (mapperID) {
    case 1:  return cpuWriteMapper1(addr, data);
    case 2:  return cpuWriteMapper2(addr, data);
    case 3:  return cpuWriteMapper3(addr, data);
    case 4:  return cpuWriteMapper4(addr, data);
    case 7:  return cpuWriteMapper7(addr, data);
    case 66: return cpuWriteMapper66(addr, data);
    }
    return false;
}

bool Cartridge::ppuRead(Word addr, Byte& data) {
    addr &= 0x1FFF;
    switch (mapperID) {
    case 0:  return ppuReadMapper0(addr, data);
    case 1:  return ppuReadMapper1(addr, data);
    case 2:  return ppuReadMapper2(addr, data);
    case 3:  return ppuReadMapper3(addr, data);
    case 4:  return ppuReadMapper4(addr, data);
    case 7:  return ppuReadMapper7(addr, data);
    case 66: return ppuReadMapper66(addr, data);
    }
    if (addr < CHR.size()) { data = CHR[addr]; return true; }
    return false;
}

bool Cartridge::ppuWrite(Word addr, Byte data) {
    addr &= 0x1FFF;
    switch (mapperID) {
    case 1:  return ppuWriteMapper1(addr, data);
    case 2:  return ppuWriteMapper2(addr, data);
    case 4:  return ppuWriteMapper4(addr, data);
    }
    if (nCHRBanks == 0 && addr < CHR.size()) { CHR[addr] = data; return true; }
    return false;
}

// ============ Mapper 0 (NROM) ============
bool Cartridge::cpuReadMapper0(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        Word mapped = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        data = PRG[mapped];
        return true;
    }
    return false;
}
bool Cartridge::ppuReadMapper0(Word addr, Byte& data) {
    if (addr < CHR.size()) { data = CHR[addr]; return true; }
    return false;
}

// ============ Mapper 1 (MMC1) ============
bool Cartridge::cpuReadMapper1(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        int prgMode = (controlReg >> 2) & 3;
        int num16 = (int)(PRG.size() / 0x4000);
        int bank = 0, offset = 0;

        if (prgMode <= 1) {
            bank = (prgBank & 0x0E) >> 1;
            offset = (bank * 0x8000) + (addr - 0x8000);
        } else if (prgMode == 2) {
            if (addr < 0xC000) offset = addr - 0x8000;
            else { bank = prgBank & 0x0F; if (bank >= num16) bank = num16 - 1; offset = (bank * 0x4000) + (addr - 0xC000); }
        } else {
            if (addr < 0xC000) { bank = prgBank & 0x0F; if (bank >= num16) bank = num16 - 1; offset = (bank * 0x4000) + (addr - 0x8000); }
            else offset = ((num16 - 1) * 0x4000) + (addr - 0xC000);
        }
        if (offset >= 0 && offset < (int)PRG.size()) { data = PRG[offset]; return true; }
        data = 0; return true;
    }
    return false;
}

bool Cartridge::cpuWriteMapper1(Word addr, Byte data) {
    if (addr >= 0x8000) {
        if (data & 0x80) { shiftReg = 0; shiftCount = 0; return true; }
        shiftReg |= ((data & 1) << shiftCount);
        shiftCount++;
        if (shiftCount == 5) {
            Byte value = shiftReg & 0x1F;
            if (addr < 0xA000) {
                controlReg = value;
                switch (controlReg & 3) {
                case 0: mirror = ONESCREEN_LO; break;
                case 1: mirror = ONESCREEN_HI; break;
                case 2: mirror = VERTICAL; break;
                case 3: mirror = HORIZONTAL; break;
                }
            }
            else if (addr < 0xC000) chrBank0 = value;
            else if (addr < 0xE000) chrBank1 = value;
            else prgBank = value;
            shiftReg = 0;
            shiftCount = 0;
        }
        return true;
    }
    return false;
}

bool Cartridge::ppuReadMapper1(Word addr, Byte& data) {
    int chrMode = (controlReg >> 4) & 1;
    int num4 = (int)(CHR.size() / 0x1000);
    int offset;
    if (chrMode == 0) {
        int bank = (chrBank0 & 0x1E) >> 1;
        if (bank >= num4) bank = num4 - 1;
        offset = (bank * 0x2000) + addr;
    } else {
        if (addr < 0x1000) {
            int bank = chrBank0 & 0x1F;
            if (bank >= num4) bank = num4 - 1;
            offset = (bank * 0x1000) + addr;
        } else {
            int bank = chrBank1 & 0x1F;
            if (bank >= num4) bank = num4 - 1;
            offset = (bank * 0x1000) + (addr - 0x1000);
        }
    }
    if (offset >= 0 && offset < (int)CHR.size()) { data = CHR[offset]; return true; }
    data = 0; return true;
}

bool Cartridge::ppuWriteMapper1(Word addr, Byte data) {
    if (nCHRBanks == 0 && addr < CHR.size()) { CHR[addr] = data; return true; }
    return false;
}

// ============ Mapper 2 (UxROM) ============
bool Cartridge::cpuReadMapper2(Word addr, Byte& data) {
    int num16 = (int)(PRG.size() / 0x4000);
    int bank;
    if (addr < 0xC000) bank = prgBank & 0x0F;
    else bank = num16 - 1;
    if (bank >= num16) bank = num16 - 1;
    int offset = (bank * 0x4000) + (addr & 0x3FFF);
    if (offset >= 0 && offset < (int)PRG.size()) { data = PRG[offset]; return true; }
    data = 0; return true;
}
bool Cartridge::cpuWriteMapper2(Word addr, Byte data) {
    if (addr >= 0x8000) { prgBank = data & 0x0F; return true; }
    return false;
}
bool Cartridge::ppuReadMapper2(Word addr, Byte& data) {
    if (addr < CHR.size()) { data = CHR[addr]; return true; }
    return false;
}
bool Cartridge::ppuWriteMapper2(Word addr, Byte data) {
    if (nCHRBanks == 0 && addr < CHR.size()) { CHR[addr] = data; return true; }
    return false;
}

// ============ Mapper 3 (CNROM) ============
bool Cartridge::cpuReadMapper3(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        int offset = (int)(addr - 0x8000) & 0x7FFF;
        if (offset < (int)PRG.size()) { data = PRG[offset]; return true; }
        data = 0; return true;
    }
    return false;
}
bool Cartridge::cpuWriteMapper3(Word addr, Byte data) {
    if (addr >= 0x8000) { prgBank = data & 0x03; return true; }
    return false;
}
bool Cartridge::ppuReadMapper3(Word addr, Byte& data) {
    int num8 = (int)(CHR.size() / 0x2000);
    int bank = prgBank & 0x03;
    if (bank >= num8) bank = num8 - 1;
    int offset = (bank * 0x2000) + (addr & 0x1FFF);
    if (offset >= 0 && offset < (int)CHR.size()) { data = CHR[offset]; return true; }
    data = 0; return true;
}

// ============ Mapper 4 (MMC3) ============
bool Cartridge::cpuReadMapper4(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        int num8 = (int)(PRG.size() / 0x2000);
        int prgMode = (mmc3BankSelect >> 6) & 1;
        int bank;

        if (prgMode == 0) {
            // $8000-$9FFF: R6 (swappable)
            // $A000-$BFFF: R7 (swappable)
            // $C000-$DFFF: second-to-last (fixed)
            // $E000-$FFFF: last (fixed)
            if (addr < 0xA000) bank = mmc3BankRegs[6] & 0x3F;
            else if (addr < 0xC000) bank = mmc3BankRegs[7] & 0x3F;
            else if (addr < 0xE000) bank = num8 - 2;
            else bank = num8 - 1;
        } else {
            // $8000-$9FFF: second-to-last (fixed)
            // $A000-$BFFF: R7 (swappable)
            // $C000-$DFFF: R6 (swappable)
            // $E000-$FFFF: last (fixed)
            if (addr < 0xA000) bank = num8 - 2;
            else if (addr < 0xC000) bank = mmc3BankRegs[7] & 0x3F;
            else if (addr < 0xE000) bank = mmc3BankRegs[6] & 0x3F;
            else bank = num8 - 1;
        }

        if (bank >= num8) bank = num8 - 1;

        int offset = (bank * 0x2000) + (addr & 0x1FFF);
        if (offset >= 0 && offset < (int)PRG.size()) { data = PRG[offset]; return true; }
        data = 0; return true;
    }
    return false;
}

bool Cartridge::cpuWriteMapper4(Word addr, Byte data) {
    if (addr < 0x8000) return false;

    if (addr < 0xA000) {
        if ((addr & 1) == 0) {
            mmc3BankSelect = data;
        } else {
            int idx = mmc3BankSelect & 0x07;
            mmc3BankRegs[idx] = data;
            bankWriteCount++;
        }
    } else if (addr < 0xC000) {
        if ((addr & 1) == 0) {
            mirror = (data & 1) ? HORIZONTAL : VERTICAL;
        }
        // odd: PRG RAM protect ? ignore
    } else if (addr < 0xE000) {
        if ((addr & 1) == 0) {
            mmc3IRQLatch = data;
        } else {
            mmc3IRQCounter = 0;
            mmc3IRQReload = true;
        }
    } else {
        if ((addr & 1) == 0) {
            mmc3IRQEnabled = false;
            mmc3IRQPending = false;
            irqPending = false;
        } else {
            mmc3IRQEnabled = true;
        }
    }
    return true;
}

bool Cartridge::ppuReadMapper4(Word addr, Byte& data) {
    int num1 = (int)(CHR.size() / 0x400);
    int chrMode = (mmc3BankSelect >> 7) & 1;
    int bank, offset;

    if (chrMode == 0) {
        // Mode 0: two 2KB + four 1KB
        // $0000-$07FF: R0 (2KB)
        // $0800-$0FFF: R1 (2KB)
        // $1000-$13FF: R2, $1400-$17FF: R3, $1800-$1BFF: R4, $1C00-$1FFF: R5
        if (addr < 0x0800) {
            bank = (mmc3BankRegs[0] & 0xFE);
            offset = (bank * 0x400) + (addr & 0x7FF);
        } else if (addr < 0x1000) {
            bank = (mmc3BankRegs[1] & 0xFE);
            offset = (bank * 0x400) + (addr & 0x7FF);
        } else if (addr < 0x1400) {
            bank = mmc3BankRegs[2];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x1800) {
            bank = mmc3BankRegs[3];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x1C00) {
            bank = mmc3BankRegs[4];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else {
            bank = mmc3BankRegs[5];
            offset = (bank * 0x400) + (addr & 0x3FF);
        }
    } else {
        // Mode 1: four 1KB + two 2KB
        // $0000-$03FF: R2, $0400-$07FF: R3, $0800-$0BFF: R4, $0C00-$0FFF: R5
        // $1000-$17FF: R0 (2KB), $1800-$1FFF: R1 (2KB)
        if (addr < 0x0400) {
            bank = mmc3BankRegs[2];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x0800) {
            bank = mmc3BankRegs[3];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x0C00) {
            bank = mmc3BankRegs[4];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x1000) {
            bank = mmc3BankRegs[5];
            offset = (bank * 0x400) + (addr & 0x3FF);
        } else if (addr < 0x1800) {
            bank = (mmc3BankRegs[0] & 0xFE);
            offset = (bank * 0x400) + (addr & 0x7FF);
        } else {
            bank = (mmc3BankRegs[1] & 0xFE);
            offset = (bank * 0x400) + (addr & 0x7FF);
        }
    }

    if (bank >= num1) bank = num1 - 1;
    if (offset >= 0 && offset < (int)CHR.size()) { data = CHR[offset]; return true; }
    data = 0; return true;
}

bool Cartridge::ppuWriteMapper4(Word addr, Byte data) {
    if (nCHRBanks == 0 && addr < CHR.size()) { CHR[addr] = data; return true; }
    return false;
}

// ============ Mapper 7 (AxROM) ============
bool Cartridge::cpuReadMapper7(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        int num32 = (int)(PRG.size() / 0x8000);
        int bank = (prgBank >> 1) & 0x07;
        if (bank >= num32) bank = num32 - 1;
        int offset = (bank * 0x8000) + (addr & 0x7FFF);
        if (offset >= 0 && offset < (int)PRG.size()) { data = PRG[offset]; return true; }
        data = 0; return true;
    }
    return false;
}
bool Cartridge::cpuWriteMapper7(Word addr, Byte data) {
    if (addr >= 0x8000) {
        prgBank = data & 0x0F;
        mirror = (data & 0x10) ? ONESCREEN_HI : ONESCREEN_LO;
        return true;
    }
    return false;
}
bool Cartridge::ppuReadMapper7(Word addr, Byte& data) {
    if (addr < CHR.size()) { data = CHR[addr]; return true; }
    return false;
}

// ============ Mapper 66 (GNROM) ============
bool Cartridge::cpuReadMapper66(Word addr, Byte& data) {
    if (addr >= 0x8000) {
        int num32 = (int)(PRG.size() / 0x8000);
        if (num32 == 0) num32 = 1;
        int bank = (gnromByte >> 4) & 0x03;
        if (bank >= num32) bank = num32 - 1;
        int offset = (bank * 0x8000) + (addr - 0x8000);
        if (offset >= 0 && offset < (int)PRG.size()) { data = PRG[offset]; return true; }
        data = 0; return true;
    }
    return false;
}
bool Cartridge::cpuWriteMapper66(Word addr, Byte data) {
    if (addr >= 0x8000) { gnromByte = data; return true; }
    return false;
}
bool Cartridge::ppuReadMapper66(Word addr, Byte& data) {
    int num8 = (int)(CHR.size() / 0x2000);
    if (num8 == 0) num8 = 1;
    int bank = gnromByte & 0x03;
    if (bank >= num8) bank = num8 - 1;
    int offset = (bank * 0x2000) + addr;
    if (offset >= 0 && offset < (int)CHR.size()) { data = CHR[offset]; return true; }
    data = 0; return true;
}

// ============ MMC3 IRQ clock (called on PPU A12 rising edge) ============
void Cartridge::clockIRQCounter() {
    if (mapperID != 4) return;
    if (mmc3IRQCounter == 0 || mmc3IRQReload) {
        mmc3IRQCounter = mmc3IRQLatch;
        mmc3IRQReload = false;
    } else {
        mmc3IRQCounter--;
    }
    if (mmc3IRQCounter == 0 && mmc3IRQEnabled) {
        mmc3IRQPending = true;
        irqPending = true;
    }
}

// ============ Serialize ============
void Cartridge::serialize(Serializer& s) {
    s.field(mirror);
    s.field(controlReg);
    s.field(chrBank0); s.field(chrBank1); s.field(prgBank);
    s.field(shiftReg); s.field(shiftCount);
    s.field(gnromByte);
    s.field(mmc3BankSelect);
    for (int i = 0; i < 8; i++) s.field(mmc3BankRegs[i]);
    s.field(mmc3IRQLatch);
    s.field(mmc3IRQCounter);
    s.field(mmc3IRQReload);
    s.field(mmc3IRQEnabled);
    s.field(mmc3IRQPending);
    s.vec(prgRam);
}
void Cartridge::saveBatteryRam(const std::string& romPath) {
    if (prgRam.empty()) return;
    std::string savPath = romPath + ".sav";
    std::ofstream f(savPath, std::ios::binary);
    if (!f) return;
    f.write(reinterpret_cast<const char*>(prgRam.data()), prgRam.size());
    // silent
}

void Cartridge::loadBatteryRam(const std::string& romPath) {
    if (prgRam.empty()) return;
    std::string savPath = romPath + ".sav";
    std::ifstream f(savPath, std::ios::binary);
    if (!f) return;
    f.read(reinterpret_cast<char*>(prgRam.data()), prgRam.size());
    // silent
}