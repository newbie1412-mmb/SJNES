#pragma once
#include "Mapper.h"
#include <array>
#include <QString>

extern "C" {
#include "emu2413.h"
}

class Mapper_085 : public Mapper
{
public:
    Mapper_085(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_085();

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    MIRROR mirror() override { return mirrorMode; }
    void GetVrc7DebugChannels(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    void GetVrc7DebugPeriods(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    void GetVrc7DebugPhases(
        float& ch1, float& ch2, float& ch3,
        float& ch4, float& ch5, float& ch6
    );
    float GetExpansionAudio() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;

private:
    std::array<uint8_t, 3> prgBank{};
    std::array<uint8_t, 8> chrBank{};

    MIRROR mirrorMode = VERTICAL;

    bool irqEnabled = false;
    bool irqEnableAfterAck = false;
    bool irqPending = false;
    bool irqCycleMode = false;
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int irqPrescaler = 341;
    uint32_t totalIrqStepCalls = 0;
    uint32_t lastFireCycle = 0;
    OPLL* opll = nullptr;

    uint8_t audioAddr = 0;
    uint8_t audioRegs[0x40]{};

    float lastVrc7Sample = 0.0f;
    float lastVrc7Debug[6]{};
    float lastVrc7Phase[6]{};
    float lastVrc7Period[6]{};
    bool lastVrc7PhaseValid[6]{};
};