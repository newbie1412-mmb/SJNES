#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>
class Mapper_024 : public Mapper {
public:
    Mapper_024(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_024();
    bool smoothSawEnabled = false;
    void setSmoothSaw(bool enable)
    {
        smoothSawEnabled = enable;
    }
    bool muteVRC6 = false;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    MIRROR mirror() override;
    void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) override;
    void GetVRC6DebugPeriods(float& ch1, float& ch2, float& ch3) const;
    void GetVRC6DebugDuty(float& ch1, float& ch2) const;
    void irqStep() override;
    bool irqState() override;
    void irqClear() override;
    std::string GetDebugInfo() override;
    float GetExpansionAudio() override;
    void GetExpansionAudioStereo(float& left, float& right) override;
    uint8_t GetNtSource(int index) const;
private:
    uint8_t prg16Bank = 0; // $8000-$BFFF, 16KB
    uint8_t prg8Bank = 0; // $C000-$DFFF, 8KB
    uint8_t chrBank[8] = {};
    uint8_t b003 = 0x20;
    uint8_t ppuBankingMode = 0;       // bits 0-1
    bool useChrRomNametables = false; // bit 4
    bool chrA10Control = true;        // bit 5
    bool prgRamEnable = false;        // bit 7
    MIRROR mirrorMode = MIRROR::VERTICAL;
    uint8_t ntSource[4] = { 0, 1, 0, 1 };
    uint8_t ntChrBank[4] = { 0, 0, 0, 0 };

    // IRQ
    bool irqActive = false;
    bool irqEnable = false;
    bool irqEnableAfterAck = false;
    bool irqModeCycle = false;
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int irqPrescaler = 341;

    struct VRC6Pulse
    {
        uint8_t volume = 0;
        uint8_t duty = 0;
        bool mode = false;
        bool enable = false;

        uint16_t period = 0;
        uint16_t timer = 0;
        uint8_t dutyPos = 0;

        float output = 0.0f;
    };

    struct VRC6Saw
    {
        uint8_t rate = 0;
        bool enable = false;
        uint16_t period = 0;
        uint16_t timer = 0;
        uint8_t step = 0;
        uint8_t accumulator = 0;
        uint16_t debugTimer = 0;
        uint8_t  debugStep = 0;
        float    debugCyclePeak = 0.0f; // biên độ ĐÓNG BĂNG trong suốt 1 chu kỳ 14-step,        
        uint16_t debugSubstepPeriod = 0; 
        float phase = 0.0f;
        float output = 0.0f;
        float prev_output = 0.0f;
        float filtered_output = 0.0f;
        float GetOutput() const
        {
            if (!enable || period < 8)
                return 0.0f;

            float frac = static_cast<float>(period - timer) / static_cast<float>(period + 1);

            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;

            return prev_output + (output - prev_output) * frac;
        }
        float GetCleanOutput(bool smooth)
        {
            if (!enable || period < 8)
            {
                filtered_output = 0.0f;
                return 0.0f;
            }

            return output;
        }
    };

    VRC6Pulse pulse1;
    VRC6Pulse pulse2;
    VRC6Saw saw;

    void UpdateB003(uint8_t data);

    uint8_t GetChrRegisterForPpuAddress(uint16_t addr) const;
    uint8_t GetChrBankForPpuAddress(uint16_t addr) const;

    void ClockVRC6Audio();
    void WritePulse(VRC6Pulse& p, uint16_t reg, uint8_t data);
    void WriteSaw(uint16_t reg, uint8_t data);
};