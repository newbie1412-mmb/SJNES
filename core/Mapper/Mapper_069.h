#pragma once
#include "Mapper.h"
#include <array>
#include <QString>

class Mapper_069 : public Mapper
{
public:
    Mapper_069(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    float GetExpansionAudio() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;
    void GetS5BDebugPeriods(float& ch1, float& ch2, float& ch3) const;
    void GetS5BDebugDuty(float& ch1, float& ch2, float& ch3) const;
    void GetExpansionAudioStereo(float& expL, float& expR) override;
    int GetMirrorMode() const { return mirrorMode; }
    bool IsPrg6000RamSelected() const { return prg6000RamSelected; }
    bool IsPrg6000RamEnabled() const { return prg6000RamEnabled; }
private:
    uint8_t command = 0x00;

    std::array<uint8_t, 8> chrBank{};
    std::array<uint8_t, 4> prgBank{};

    int mirrorMode = 0;

    bool irqEnabled = false;
    bool irqCounterEnabled = false;
    bool irqPending = false;
    uint16_t irqCounter = 0;

    // Sunsoft 5B audio
    uint8_t audioRegSelect = 0;
    uint8_t audioRegs[16]{};

    int toneCounter[3]{};
    bool toneOutput[3]{};
    float lastToneSample[3]{};

    int audioDivider = 0;
    bool prg6000RamSelected = false;
    bool prg6000RamEnabled = false;
};