#pragma once
#include "types.h"

class APU {
public:
    APU();
    void setPAL(bool pal) { cpuFreq = pal ? 1662607 : 1789773; }
    void cpuWrite(Word addr, Byte data);
    Byte cpuRead(Word addr);
    void clock();
    int drain(int16_t* buf, int maxCount);
    void serialize(class Serializer& s);

private:
    int cpuFreq = 1789773;
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int RING_SIZE = 32768;

    int16_t ring[RING_SIZE] = {};
    int ringHead = 0;
    int ringTail = 0;
    long long sampleAccum = 0;

    float lpf1 = 0.0f;
    float lpf2 = 0.0f;
    float hpfPrevIn = 0.0f;
    float hpfPrevOut = 0.0f;
    float mixAccum = 0.0f;
    int mixCount = 0;
    bool oddCycle = false;

    int frameCounter = 0;
    int frameMode = 0;

    struct Pulse {
        int timer = 0;
        int timerPeriod = 0;
        int seqStep = 0;
        int length = 0;
        bool halt = false;
        bool constVol = false;
        Byte volume = 0;
        Byte duty = 0;
        bool envStart = false;
        int envDivider = 0;
        Byte envDecay = 0;
        bool sweepEnabled = false;
        int sweepDivider = 0;
        bool sweepReload = false;
        Byte sweepPeriod = 0;
        bool sweepNegate = false;
        Byte sweepShift = 0;
        bool onesComp = false;
        bool enabled = false;
    } pulse1, pulse2;

    struct Triangle {
        int timer = 0;
        int timerPeriod = 0;
        int seqStep = 0;
        int length = 0;
        bool halt = false;
        bool linearReloadFlag = false;
        int linearCounter = 0;
        Byte linearReloadVal = 0;
        bool enabled = false;
    } triangle;

    struct Noise {
        int timer = 0;
        int timerPeriod = 4;
        Word shiftReg = 1;
        bool mode = false;
        int length = 0;
        bool halt = false;
        bool constVol = false;
        Byte volume = 0;
        bool envStart = false;
        int envDivider = 0;
        Byte envDecay = 0;
        bool enabled = false;
    } noise;

    static const Byte lengthTable[32];
    static const Word noisePeriodTable[16];
    static const Byte dutySeq[4][8];
    static const Byte triangleSeq[32];

    void clockPulse(Pulse& p);
    void clockTriangle();
    void clockNoise();
    void quarterFrame();
    void halfFrame();
    void sweepStep(Pulse& p);
    int pulseOutput(const Pulse& p);
    int noiseOutput();
    void writePulse(Pulse& p, Word addr, Byte data);
    void writeTriangle(Word addr, Byte data);
    void writeNoise(Word addr, Byte data);
};