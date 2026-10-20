#pragma once
#include "types.h"
#include <cstdint>

class Cartridge;

class PPU {
public:
    PPU();
    void ConnectCartridge(Cartridge* c) { cart = c; }
    void cpuWrite(Word addr, Byte data);
    Byte cpuRead(Word addr, bool readOnly = false);
    void ppuWrite(Word addr, Byte data);
    Byte ppuRead(Word addr, bool readOnly = false);
    void clock();
    void setPAL(bool p) { isPAL = p; }
    void serialize(class Serializer& s);

    // non-inline getters, defined in ppu.cpp ? no inlining, no caching
    Byte GetCtrl();
    Byte GetStatus();
    int GetScanline();
    int GetCycle();
    unsigned long long GetClockCount();
    Byte GetMask();

    uint32_t frame[256 * 240];
    bool nmi = false;
    bool lastA12 = false;

private:
    Cartridge* cart = nullptr;

    Byte tblName[2][1024]{};
    Byte tblPalette[32]{};
    Byte oam[256]{};

    Word v_reg = 0, t_reg = 0;

    Byte v_coarse_x() const { return v_reg & 0x1F; }
    void set_v_coarse_x(Byte v) { v_reg = (v_reg & ~0x1F) | (v & 0x1F); }
    Byte v_coarse_y() const { return (v_reg >> 5) & 0x1F; }
    void set_v_coarse_y(Byte v) { v_reg = (v_reg & ~(0x1F << 5)) | ((v & 0x1F) << 5); }
    Byte v_nt_x() const { return (v_reg >> 10) & 1; }
    void set_v_nt_x(Byte v) { v_reg = (v_reg & ~(1 << 10)) | ((v & 1) << 10); }
    Byte v_nt_y() const { return (v_reg >> 11) & 1; }
    void set_v_nt_y(Byte v) { v_reg = (v_reg & ~(1 << 11)) | ((v & 1) << 11); }
    Byte v_fine_y() const { return (v_reg >> 12) & 7; }
    void set_v_fine_y(Byte v) { v_reg = (v_reg & ~(7 << 12)) | ((v & 7) << 12); }

    Byte t_coarse_x() const { return t_reg & 0x1F; }
    void set_t_coarse_x(Byte v) { t_reg = (t_reg & ~0x1F) | (v & 0x1F); }
    Byte t_coarse_y() const { return (t_reg >> 5) & 0x1F; }
    void set_t_coarse_y(Byte v) { t_reg = (t_reg & ~(0x1F << 5)) | ((v & 0x1F) << 5); }
    Byte t_nt_x() const { return (t_reg >> 10) & 1; }
    void set_t_nt_x(Byte v) { t_reg = (t_reg & ~(1 << 10)) | ((v & 1) << 10); }
    Byte t_nt_y() const { return (t_reg >> 11) & 1; }
    void set_t_nt_y(Byte v) { t_reg = (t_reg & ~(1 << 11)) | ((v & 1) << 11); }
    Byte t_fine_y() const { return (t_reg >> 12) & 7; }
    void set_t_fine_y(Byte v) { t_reg = (t_reg & ~(7 << 12)) | ((v & 7) << 12); }

    Byte fine_x = 0;
    bool write_latch = false;
    Byte ppu_ctrl = 0, ppu_mask = 0, ppu_status = 0, oam_addr = 0;
    Byte data_buffer = 0;

    int scanline = 0;
    int cycle = 0;
    unsigned long long clockCount = 0;

    struct VramWrite { Word addr; Byte data; };
    VramWrite vramBuf[256];
    int vramBufCount = 0;

    void renderPixel();
    void writePixel(int x, int y, uint32_t color);
    uint32_t paletteColor(Byte idx);

    void incX();
    void incY();
    void loadBackgroundShifters();
    void updateShifters();

    Word bgShiftPatLo = 0, bgShiftPatHi = 0;
    Word bgShiftAttLo = 0, bgShiftAttHi = 0;
    bool isPAL = false;
    Byte ntByte = 0, atByte = 0, bgLo = 0, bgHi = 0;

    struct SpriteOut { Byte patLo = 0, patHi = 0, attr = 0, x = 0; Byte oamIdx = 0xFF; bool valid = false; };
    Byte spriteCount = 0;
    SpriteOut spriteBuf[8];
};
