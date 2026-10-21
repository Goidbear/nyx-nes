#include <cstdio>
#include "ppu.h"
#include "cartridge.h"
#include "savestate.h"

PPU::PPU() {}

Byte PPU::cpuRead(Word addr, bool readOnly) {
    Byte data = 0;
    switch (addr) {
    case 0x0002:
        data = (ppu_status & 0xE0) | (data_buffer & 0x1F);
        ppu_status &= ~0x80;
        write_latch = false;
        break;
    case 0x0004: data = oam[oam_addr]; break;
    case 0x0007:
        data = data_buffer;
        data_buffer = ppuRead(v_reg & 0x3FFF, readOnly);
        if ((v_reg & 0x3FFF) >= 0x3F00) data = data_buffer;
        v_reg += (ppu_ctrl & 0x04) ? 32 : 1;
        break;
    }
    return data;
}

void PPU::cpuWrite(Word addr, Byte data) {
    switch (addr) {
    case 0x0000:
        ppu_ctrl = data;
        set_t_nt_x((data >> 0) & 1);
        set_t_nt_y((data >> 1) & 1);
        break;
    case 0x0001: ppu_mask = data; break;
    case 0x0003: oam_addr = data; break;
    case 0x0004: oam[oam_addr++] = data; break;
    case 0x0005:
        if (!write_latch) {
            fine_x = data & 0x07;
            set_t_coarse_x(data >> 3);
            write_latch = true;
        } else {
            set_t_fine_y(data & 0x07);
            set_t_coarse_y(data >> 3);
            write_latch = false;
        }
        break;
    case 0x0006:
        if (!write_latch) {
            t_reg = (t_reg & 0x00FF) | ((data & 0x3F) << 8);
            write_latch = true;
        } else {
            t_reg = (t_reg & 0xFF00) | data;
            v_reg = t_reg;
            write_latch = false;
        }
        break;
    case 0x0007: {
        Word a = v_reg & 0x3FFF;
        if ((ppu_mask & 0x18) && scanline >= 0 && scanline < 240 && cycle < 257
            && a >= 0x2000 && a <= 0x3EFF) {
            if (vramBufCount < 256) {
                vramBuf[vramBufCount].addr = a;
                vramBuf[vramBufCount].data = data;
                vramBufCount++;
            } else {
                ppuWrite(a, data);
            }
        } else {
            ppuWrite(a, data);
        }
        v_reg += (ppu_ctrl & 0x04) ? 32 : 1;
    } break;
    }
}

Byte PPU::ppuRead(Word addr, bool) {
    addr &= 0x3FFF;
    if (addr <= 0x1FFF) {
        Byte d = 0; if (cart) cart->ppuRead(addr, d);
        return d;
    }
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        addr &= 0x0FFF;
        if (cart && cart->mirror == Cartridge::ONESCREEN_LO) return tblName[0][addr & 0x03FF];
        if (cart && cart->mirror == Cartridge::ONESCREEN_HI) return tblName[1][addr & 0x03FF];
        if (cart && cart->mirror == Cartridge::VERTICAL) {
            if (addr <= 0x03FF) return tblName[0][addr & 0x03FF];
            if (addr <= 0x07FF) return tblName[1][addr & 0x03FF];
            if (addr <= 0x0BFF) return tblName[0][addr & 0x03FF];
            return tblName[1][addr & 0x03FF];
        } else {
            if (addr <= 0x03FF) return tblName[0][addr & 0x03FF];
            if (addr <= 0x07FF) return tblName[0][addr & 0x03FF];
            if (addr <= 0x0BFF) return tblName[1][addr & 0x03FF];
            return tblName[1][addr & 0x03FF];
        }
    }
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        return tblPalette[addr] & (ppu_mask & 0x01 ? 0x30 : 0x3F);
    }
    return 0;
}

void PPU::ppuWrite(Word addr, Byte data) {
    addr &= 0x3FFF;
    if (addr <= 0x1FFF) { if (cart) cart->ppuWrite(addr, data); return; }
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        addr &= 0x0FFF;
        if (cart && cart->mirror == Cartridge::ONESCREEN_LO) { tblName[0][addr & 0x03FF] = data; return; }
        if (cart && cart->mirror == Cartridge::ONESCREEN_HI) { tblName[1][addr & 0x03FF] = data; return; }
        if (cart && cart->mirror == Cartridge::VERTICAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[1][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[0][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        } else {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[1][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        return;
    }
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        tblPalette[addr] = data;
    }
}

void PPU::incX() {
    if (v_coarse_x() == 31) { set_v_coarse_x(0); set_v_nt_x(v_nt_x() ^ 1); }
    else set_v_coarse_x(v_coarse_x() + 1);
}
void PPU::incY() {
    if (v_fine_y() < 7) set_v_fine_y(v_fine_y() + 1);
    else {
        set_v_fine_y(0);
        if (v_coarse_y() == 29) { set_v_coarse_y(0); set_v_nt_y(v_nt_y() ^ 1); }
        else if (v_coarse_y() == 31) set_v_coarse_y(0);
        else set_v_coarse_y(v_coarse_y() + 1);
    }
}

void PPU::loadBackgroundShifters() {
    bgShiftPatLo = (bgShiftPatLo & 0xFF00) | bgLo;
    bgShiftPatHi = (bgShiftPatHi & 0xFF00) | bgHi;
    bgShiftAttLo = (bgShiftAttLo & 0xFF00) | ((atByte & 0x01) ? 0xFF : 0x00);
    bgShiftAttHi = (bgShiftAttHi & 0xFF00) | ((atByte & 0x02) ? 0xFF : 0x00);
}
void PPU::updateShifters() {
    if (ppu_mask & 0x08) {
        bgShiftPatLo <<= 1; bgShiftPatHi <<= 1;
        bgShiftAttLo <<= 1; bgShiftAttHi <<= 1;
    }
}

void PPU::clock() {
    clockCount++;

    auto cycleVisible = [&]() { return scanline >= 0 && scanline < 240; };

    if (scanline >= -1 && scanline < 240) {
        if (scanline == -1 && cycle == 1) { ppu_status &= ~0xE0; vramBufCount = 0; }

        if ((ppu_mask & 0x18) && ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338))) {
            updateShifters();
            switch ((cycle - 1) % 8) {
            case 0:
                loadBackgroundShifters();
                ntByte = ppuRead(0x2000 | (v_reg & 0x0FFF));
                break;
            case 2:
                atByte = ppuRead(0x23C0 | (v_nt_y() << 11) | (v_nt_x() << 10)
                    | ((v_coarse_y() >> 2) << 3) | (v_coarse_x() >> 2));
                if (v_coarse_y() & 0x02) atByte >>= 4;
                if (v_coarse_x() & 0x02) atByte >>= 2;
                atByte &= 0x03;
                break;
            case 4:
                bgLo = ppuRead(((ppu_ctrl & 0x10) << 8) + (ntByte << 4) + v_fine_y());
                break;
            case 6:
                bgHi = ppuRead(((ppu_ctrl & 0x10) << 8) + (ntByte << 4) + v_fine_y() + 8);
                break;
            case 7:
                incX();
                break;
            }
        }

        if (cycle == 256 && (ppu_mask & 0x18)) incY();
        if (cycle == 257) {
            for (int i = 0; i < vramBufCount; i++) ppuWrite(vramBuf[i].addr, vramBuf[i].data);
            vramBufCount = 0;
            loadBackgroundShifters();
            if (ppu_mask & 0x18) {
                set_v_nt_x(t_nt_x());
                set_v_coarse_x(t_coarse_x());
            }

            int spriteScan = scanline + 1;
            if (spriteScan >= 0 && spriteScan < 240) {
                int spriteHeight = (ppu_ctrl & 0x20) ? 16 : 8;
                spriteCount = 0;
                for (int i = 0; i < 64; i++) {
                    Byte spriteY = oam[i * 4 + 0];
                    int diff = spriteScan - (spriteY + 1);
                    if (diff < 0 || diff >= spriteHeight) continue;
                    if (spriteCount >= 8) { ppu_status |= 0x20; break; }
                    Byte spriteTile = oam[i * 4 + 1];
                    Byte spriteAttr = oam[i * 4 + 2];
                    Byte patLo, patHi;
                    if (spriteHeight == 8) {
                        Byte row = (spriteAttr & 0x80) ? (7 - diff) : diff;
                        Word base = ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) + (spriteTile << 4);
                        patLo = ppuRead(base + row);
                        patHi = ppuRead(base + row + 8);
                    } else {
                        Word patTable = (spriteTile & 1) ? 0x1000 : 0x0000;
                        Byte tileBase = spriteTile & 0xFE;
                        Byte row = (spriteAttr & 0x80) ? (15 - diff) : diff;
                        Byte tile = (row < 8) ? tileBase : (tileBase + 1);
                        Byte r = (row < 8) ? row : (row - 8);
                        Word base = patTable + (tile << 4);
                        patLo = ppuRead(base + r);
                        patHi = ppuRead(base + r + 8);
                    }
                    spriteBuf[spriteCount].patLo = patLo;
                    spriteBuf[spriteCount].patHi = patHi;
                    spriteBuf[spriteCount].attr = spriteAttr;
                    spriteBuf[spriteCount].x = oam[i * 4 + 3];
                    spriteBuf[spriteCount].oamIdx = (Byte)i;
                    spriteBuf[spriteCount].valid = true;
                    spriteCount++;
                }
            } else {
                spriteCount = 0;
            }
        }
        // MMC3 IRQ: A12 rising edge. Fires at the exact hardware cycle.
        // If sprite pattern table is $1000 (PPUCTRL bit 3), A12 rises at cycle 260 (sprite fetch).
        // If BG pattern table is $1000 (PPUCTRL bit 4), A12 rises at cycle 324 (BG fetch).
        if (cart) {
            if (cycle == 260 && (ppu_ctrl & 0x08)) cart->clockIRQCounter();
            if (cycle == 324 && (ppu_ctrl & 0x10)) cart->clockIRQCounter();
        }
        if (cycle == 338 || cycle == 340) ntByte = ppuRead(0x2000 | (v_reg & 0x0FFF));
        if ((ppu_mask & 0x18) && scanline == -1 && cycle >= 280 && cycle < 305) {
            set_v_fine_y(t_fine_y());
            set_v_nt_y(t_nt_y());
            set_v_coarse_y(t_coarse_y());
        }
    }

    if (scanline == 241 && cycle == 1) {
        ppu_status |= 0x80;
        if (ppu_ctrl & 0x80) nmi = true;
    }

    if (cycleVisible() && cycle >= 1 && cycle <= 256) renderPixel();

    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= (isPAL ? 311 : 261)) scanline = -1;
    }
}

void PPU::renderPixel() {
    int x = cycle - 1;
    int y = scanline;

    Byte bg = 0; Byte pal = 0;

    bool bgLeftOK = (x >= 8) || (ppu_mask & 0x02);
    if ((ppu_mask & 0x08) && bgLeftOK) {
        Word bit_mux = 0x8000 >> fine_x;
        Byte p0 = (bgShiftPatLo & bit_mux) ? 1 : 0;
        Byte p1 = (bgShiftPatHi & bit_mux) ? 1 : 0;
        bg = (p1 << 1) | p0;
        Byte a0 = (bgShiftAttLo & bit_mux) ? 1 : 0;
        Byte a1 = (bgShiftAttHi & bit_mux) ? 1 : 0;
        pal = (a1 << 1) | a0;
    }

    Byte spr = 0; Byte sprPal = 0; bool sprPriority = false;
    bool sprLeftOK = (x >= 8) || (ppu_mask & 0x04);
    if ((ppu_mask & 0x10) && sprLeftOK) {
        for (int i = 0; i < spriteCount; i++) {
            int diff = x - spriteBuf[i].x;
            if (diff < 0 || diff > 7) continue;
            Byte bit = (spriteBuf[i].attr & 0x40) ? (Byte)diff : (Byte)(7 - diff);
            Byte lo = (spriteBuf[i].patLo >> bit) & 1;
            Byte hi = (spriteBuf[i].patHi >> bit) & 1;
            Byte p = (hi << 1) | lo;
            if (p) {
                if (spriteBuf[i].oamIdx == 0 && bg != 0) ppu_status |= 0x40;
                spr = p;
                sprPal = (spriteBuf[i].attr & 0x03) + 4;
                sprPriority = !(spriteBuf[i].attr & 0x20);
                break;
            }
        }
    }

    Byte colorIdx = 0; Byte palette = 0;
    if (bg && spr) {
        if (sprPriority) { colorIdx = spr; palette = sprPal; }
        else { colorIdx = bg; palette = pal; }
    } else if (bg) { colorIdx = bg; palette = pal; }
    else if (spr) { colorIdx = spr; palette = sprPal; }

    Byte palIdx = ppuRead(0x3F00 + palette * 4 + colorIdx);
    writePixel(x, y, paletteColor(palIdx));
}

uint32_t PPU::paletteColor(Byte idx) {
    static const uint32_t pal[64] = {
        0x747474,0x24188C,0x0000A8,0x44009C,0x8C0074,0xA80010,0xA40000,0x7C2C00,
        0x402C00,0x004400,0x005000,0x004400,0x002C7C,0x000000,0x000000,0x000000,
        0xBCBCBC,0x0070EC,0x2038EC,0x8000F0,0xBC00BC,0xE40058,0xD82800,0xC84C0C,
        0x887000,0x009400,0x00A800,0x009038,0x008088,0x000000,0x000000,0x000000,
        0xFCFCFC,0x3CBCFC,0x5C94FC,0xCC88FC,0xF478FC,0xFC74B4,0xFC7460,0xFC9838,
        0xF0BC3C,0x80D010,0x4CDC48,0x58F898,0x00E8D8,0x787878,0x000000,0x000000,
        0xFCFCFC,0xA8E4FC,0xC4D4FC,0xD4C8FC,0xFCC4FC,0xFCC4D8,0xFCBCB0,0xFCD8A8,
        0xFCE4A0,0xE0FCA0,0xA8F0BC,0xB0FCCC,0x9CFCF0,0xC4C4C4,0x000000,0x000000
    };
    return pal[idx & 0x3F];
}

void PPU::writePixel(int x, int y, uint32_t c) {
    if (x >= 0 && x < 256 && y >= 0 && y < 240) frame[y * 256 + x] = c;
}

Byte PPU::GetCtrl() { return ppu_ctrl; }
Byte PPU::GetStatus() { return ppu_status; }
int PPU::GetScanline() { return scanline; }
int PPU::GetCycle() { return cycle; }
unsigned long long PPU::GetClockCount() { return clockCount; }
Byte PPU::GetMask() { return ppu_mask; }

void PPU::serialize(Serializer& s) {
    s.field(v_reg); s.field(t_reg);
    s.field(fine_x); s.field(write_latch);
    s.field(ppu_ctrl); s.field(ppu_mask); s.field(ppu_status); s.field(oam_addr);
    s.field(data_buffer);
    s.field(scanline); s.field(cycle);
    s.field(clockCount);
    s.array(tblName[0]); s.array(tblName[1]);
    s.array(tblPalette);
    s.array(oam);
    s.field(bgShiftPatLo); s.field(bgShiftPatHi);
    s.field(bgShiftAttLo); s.field(bgShiftAttHi);
    s.field(ntByte); s.field(atByte); s.field(bgLo); s.field(bgHi);
    s.field(spriteCount);
    for (int i = 0; i < 8; i++) {
        s.field(spriteBuf[i].patLo);
        s.field(spriteBuf[i].patHi);
        s.field(spriteBuf[i].attr);
        s.field(spriteBuf[i].x);
        s.field(spriteBuf[i].oamIdx);
        s.field(spriteBuf[i].valid);
    }
    s.field(vramBufCount);
    for (int i = 0; i < 256 && i < vramBufCount; i++) {
        s.field(vramBuf[i].addr);
        s.field(vramBuf[i].data);
    }
    s.field(nmi);
}
