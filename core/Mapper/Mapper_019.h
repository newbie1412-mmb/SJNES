#pragma once

#include "Mapper.h"
#include "Namco163Audio.h"

class Mapper_019 : public Mapper
{
public:
    Mapper_019(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mapped) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped) override;

    bool cpuReadRegister(uint16_t addr, uint8_t& data) override;

    void clock() override;
    bool irqState() override;
    void irqClear() override;
    void reset() override;

    MIRROR mirror() override
    {
        return mirroring_mode;
    }

    // Âm thanh Namco 163
    void ClockCpu(int cycles) override;
    float GetExpansionAudio() override;

    void irqStep() override;

    void GetN163DebugChannels(
        float& ch1, float& ch2, float& ch3, float& ch4,
        float& ch5, float& ch6, float& ch7, float& ch8
    );

    void GetN163DebugPeriods(
        float& ch1, float& ch2, float& ch3, float& ch4,
        float& ch5, float& ch6, float& ch7, float& ch8
    );
private:

    void UpdateNametableMirror();
    bool enableChrRomNametable = true;
    uint8_t prg[4];
    uint8_t chr[12];

    uint8_t reg_E800 = 0;

    MIRROR mirroring_mode = HARDWARE;

    // Namco 163 audio thật
    Namco163Audio n163;

    uint16_t irq_counter = 0;
    bool irq_enable = false;
    bool irq_active = false;

    static constexpr uint32_t WRITE_SINK = 0x7FFFFFFF;

    uint32_t GetCiramBase() const;
    bool BankUsesCiram(int slot, uint8_t bank) const;
    bool MapPpuAddress(uint16_t addr, uint32_t& mapped, bool forWrite) const;

};