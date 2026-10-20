#include "apu.h"
#include "savestate.h"
#include <cstring>

const Byte APU::lengthTable[32] = {
    10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};
const Word APU::noisePeriodTable[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};
const Byte APU::dutySeq[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};
const Byte APU::triangleSeq[32] = {
    15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
};

APU::APU() { pulse1.onesComp = true; }

void APU::cpuWrite(Word addr, Byte data) {
    if (addr >= 0x4000 && addr <= 0x4003) writePulse(pulse1, addr, data);
    else if (addr >= 0x4004 && addr <= 0x4007) writePulse(pulse2, addr, data);
    else if (addr >= 0x4008 && addr <= 0x400B) writeTriangle(addr, data);
    else if (addr >= 0x400C && addr <= 0x400F) writeNoise(addr, data);
    else if (addr == 0x4015) {
        pulse1.enabled = (data & 0x01) != 0;
        pulse2.enabled = (data & 0x02) != 0;
        triangle.enabled = (data & 0x04) != 0;
        noise.enabled = (data & 0x08) != 0;
        if (!pulse1.enabled) pulse1.length = 0;
        if (!pulse2.enabled) pulse2.length = 0;
        if (!triangle.enabled) triangle.length = 0;
        if (!noise.enabled) noise.length = 0;
    }
    else if (addr == 0x4017) {
        frameMode = (data >> 7) & 1;
        frameCounter = 0;
        if (data & 0x80) { quarterFrame(); halfFrame(); }
    }
}

Byte APU::cpuRead(Word addr) {
    if (addr == 0x4015) {
        Byte r = 0;
        if (pulse1.length > 0) r |= 0x01;
        if (pulse2.length > 0) r |= 0x02;
        if (triangle.length > 0) r |= 0x04;
        if (noise.length > 0) r |= 0x08;
        return r;
    }
    return 0;
}

void APU::writePulse(Pulse& p, Word addr, Byte data) {
    switch (addr & 3) {
    case 0:
        p.duty = (data >> 6) & 3;
        p.halt = (data & 0x20) != 0;
        p.constVol = (data & 0x10) != 0;
        p.volume = data & 0x0F;
        break;
    case 1:
        p.sweepEnabled = (data & 0x80) != 0;
        p.sweepPeriod = (data >> 4) & 7;
        p.sweepNegate = (data & 0x08) != 0;
        p.sweepShift = data & 7;
        p.sweepReload = true;
        break;
    case 2:
        p.timerPeriod = (p.timerPeriod & 0x700) | data;
        break;
    case 3:
        p.timerPeriod = (p.timerPeriod & 0x0FF) | ((data & 0x07) << 8);
        if (p.enabled) p.length = lengthTable[data >> 3];
        p.seqStep = 0;
        p.envStart = true;
        break;
    }
}

void APU::writeTriangle(Word addr, Byte data) {
    switch (addr & 3) {
    case 0:
        triangle.halt = (data & 0x80) != 0;
        triangle.linearReloadVal = data & 0x7F;
        break;
    case 2:
        triangle.timerPeriod = (triangle.timerPeriod & 0x700) | data;
        break;
    case 3:
        triangle.timerPeriod = (triangle.timerPeriod & 0x0FF) | ((data & 0x07) << 8);
        if (triangle.enabled) triangle.length = lengthTable[data >> 3];
        triangle.linearReloadFlag = true;
        break;
    }
}

void APU::writeNoise(Word addr, Byte data) {
    switch (addr & 3) {
    case 0:
        noise.halt = (data & 0x20) != 0;
        noise.constVol = (data & 0x10) != 0;
        noise.volume = data & 0x0F;
        break;
    case 2:
        noise.mode = (data & 0x80) != 0;
        noise.timerPeriod = noisePeriodTable[data & 0x0F] * 2;
        break;
    case 3:
        if (noise.enabled) noise.length = lengthTable[data >> 3];
        noise.envStart = true;
        break;
    }
}

// Pulse timers tick once per 2 CPU cycles (called from clock())
void APU::clockPulse(Pulse& p) {
    if (p.timer > 0) { p.timer--; return; }
    p.timer = p.timerPeriod;
    p.seqStep = (p.seqStep + 1) & 7;
}

void APU::clockTriangle() {
    if (triangle.timer > 0) { triangle.timer--; return; }
    triangle.timer = triangle.timerPeriod;
    if (triangle.length > 0 && triangle.linearCounter > 0 && triangle.timerPeriod >= 2)
        triangle.seqStep = (triangle.seqStep + 1) & 31;
}

void APU::clockNoise() {
    if (noise.timer > 0) { noise.timer--; return; }
    noise.timer = noise.timerPeriod;
    int tap = noise.mode ? 6 : 1;
    Word fb = (noise.shiftReg & 1) ^ ((noise.shiftReg >> tap) & 1);
    noise.shiftReg = (noise.shiftReg >> 1) | (fb << 14);
}

int APU::pulseOutput(const Pulse& p) {
    if (p.length == 0 || p.timerPeriod < 8) return 0;
    // Only apply sweep mute when sweep is actually enabled (real NES behaviour).
    // With sweep disabled (sweepShift=0), no mute happens.
    if (p.sweepEnabled) {
        int change = p.timerPeriod >> p.sweepShift;
        int target = p.sweepNegate ? p.timerPeriod - change - (p.onesComp ? 1 : 0)
                                   : p.timerPeriod + change;
        if (target > 0x7FF) return 0;
    }
    if (!dutySeq[p.duty][p.seqStep]) return 0;
    return p.constVol ? p.volume : p.envDecay;
}

int APU::noiseOutput() {
    if (noise.length == 0 || (noise.shiftReg & 1)) return 0;
    return noise.constVol ? noise.volume : noise.envDecay;
}

void APU::sweepStep(Pulse& p) {
    int change = p.timerPeriod >> p.sweepShift;
    int target = p.sweepNegate ? p.timerPeriod - change - (p.onesComp ? 1 : 0)
                               : p.timerPeriod + change;
    bool mute = (p.timerPeriod < 8 || target > 0x7FF);
    if (p.sweepDivider == 0 && p.sweepEnabled && p.sweepShift > 0 && !mute)
        p.timerPeriod = target;
    if (p.sweepDivider == 0 || p.sweepReload) {
        p.sweepDivider = p.sweepPeriod;
        p.sweepReload = false;
    } else {
        p.sweepDivider--;
    }
}

void APU::quarterFrame() {
    auto envStep = [](auto& c) {
        if (c.envStart) { c.envStart = false; c.envDecay = 15; c.envDivider = c.volume; }
        else if (c.envDivider > 0) c.envDivider--;
        else { c.envDivider = c.volume; if (c.envDecay > 0) c.envDecay--; else if (c.halt) c.envDecay = 15; }
    };
    envStep(pulse1); envStep(pulse2); envStep(noise);
    if (triangle.linearReloadFlag) triangle.linearCounter = triangle.linearReloadVal;
    else if (triangle.linearCounter > 0) triangle.linearCounter--;
    if (!triangle.halt) triangle.linearReloadFlag = false;
}

void APU::halfFrame() {
    auto lenStep = [](auto& c) { if (!c.halt && c.length > 0) c.length--; };
    lenStep(pulse1); lenStep(pulse2); lenStep(triangle); lenStep(noise);
    sweepStep(pulse1);
    sweepStep(pulse2);
}

void APU::clock() {
    if (oddCycle) { clockPulse(pulse1); clockPulse(pulse2); }
    oddCycle = !oddCycle;
    clockTriangle();
    clockNoise();

    frameCounter++;
    if (frameMode == 0) {
        if (frameCounter == 7457) quarterFrame();
        else if (frameCounter == 14913) { quarterFrame(); halfFrame(); }
        else if (frameCounter == 22371) quarterFrame();
        else if (frameCounter >= 29829) { quarterFrame(); halfFrame(); frameCounter = 0; }
    } else {
        if (frameCounter == 7457) quarterFrame();
        else if (frameCounter == 14913) { quarterFrame(); halfFrame(); }
        else if (frameCounter == 22371) quarterFrame();
        else if (frameCounter == 29829) quarterFrame();
        else if (frameCounter >= 37281) { quarterFrame(); halfFrame(); frameCounter = 0; }
    }

    // mix this CPU cycle
    int pulseSum = pulseOutput(pulse1) + pulseOutput(pulse2);
    float pulseOut = 0.0f;
    if (pulseSum > 0) pulseOut = 95.88f / (8128.0f / (float)pulseSum + 100.0f);

    int tri = 0;
    if (triangle.length > 0 && triangle.linearCounter > 0 && triangle.timerPeriod >= 2)
        tri = triangleSeq[triangle.seqStep];
    int noi = noiseOutput();
    float tndOut = 0.0f;
    if (tri + noi > 0) {
        float t = tri / 8227.0f + noi / 12241.0f;
        tndOut = 159.79f / (1.0f / t + 100.0f);
    }
    mixAccum += pulseOut + tndOut;
    mixCount++;

    sampleAccum += SAMPLE_RATE;
    if (sampleAccum >= cpuFreq) {
        sampleAccum -= cpuFreq;
        float mixed = mixAccum / (float)mixCount;   // average = smooth, no harsh buzz
        mixAccum = 0.0f;
        mixCount = 0;

        // High-pass: removes DC drift and low rumble
        float hpfOut = mixed - hpfPrevIn + 0.995f * hpfPrevOut;
        hpfPrevIn = mixed;
        hpfPrevOut = hpfOut;

        // Low-pass: removes aliasing from sample-rate output
        // (2nd-order, cutoff around 6 kHz — smooths high harmonics)
        lpf1 = lpf1 + 0.90f * (hpfOut - lpf1);
        lpf2 = lpf2 + 0.90f * (lpf1 - lpf2);

        int16_t s = (int16_t)(lpf2 * 18000.0f);
        ring[ringHead] = s;
        ringHead = (ringHead + 1) % RING_SIZE;
    }
}

int APU::drain(int16_t* buf, int maxCount) {
    int n = 0;
    while (n < maxCount && ringTail != ringHead) {
        buf[n++] = ring[ringTail];
        ringTail = (ringTail + 1) % RING_SIZE;
    }
    return n;
}
void APU::serialize(Serializer& s) {
    s.field(cpuFreq);
    s.field(sampleAccum);
    s.field(hpfPrevIn); s.field(hpfPrevOut);
    s.field(lpf1); s.field(lpf2);
    s.field(mixAccum); s.field(mixCount);
    s.field(oddCycle);
    s.field(frameCounter); s.field(frameMode);

    auto& p1 = pulse1; auto& p2 = pulse2;
    s.field(p1.timer); s.field(p1.timerPeriod); s.field(p1.seqStep);
    s.field(p1.length); s.field(p1.halt); s.field(p1.constVol);
    s.field(p1.volume); s.field(p1.duty); s.field(p1.envStart);
    s.field(p1.envDivider); s.field(p1.envDecay);
    s.field(p1.sweepEnabled); s.field(p1.sweepDivider); s.field(p1.sweepReload);
    s.field(p1.sweepPeriod); s.field(p1.sweepNegate); s.field(p1.sweepShift);
    s.field(p1.onesComp); s.field(p1.enabled);
    s.field(p2.timer); s.field(p2.timerPeriod); s.field(p2.seqStep);
    s.field(p2.length); s.field(p2.halt); s.field(p2.constVol);
    s.field(p2.volume); s.field(p2.duty); s.field(p2.envStart);
    s.field(p2.envDivider); s.field(p2.envDecay);
    s.field(p2.sweepEnabled); s.field(p2.sweepDivider); s.field(p2.sweepReload);
    s.field(p2.sweepPeriod); s.field(p2.sweepNegate); s.field(p2.sweepShift);
    s.field(p2.onesComp); s.field(p2.enabled);

    s.field(triangle.timer); s.field(triangle.timerPeriod); s.field(triangle.seqStep);
    s.field(triangle.length); s.field(triangle.halt);
    s.field(triangle.linearReloadFlag); s.field(triangle.linearCounter);
    s.field(triangle.linearReloadVal); s.field(triangle.enabled);

    s.field(noise.timer); s.field(noise.timerPeriod); s.field(noise.shiftReg);
    s.field(noise.mode); s.field(noise.length); s.field(noise.halt);
    s.field(noise.constVol); s.field(noise.volume); s.field(noise.envStart);
    s.field(noise.envDivider); s.field(noise.envDecay); s.field(noise.enabled);

    // reset ring buffer on load — flushing avoids stale audio
    ringHead = 0; ringTail = 0;
}