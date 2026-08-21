#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>

class Mapper_018 : public Mapper
{
public:
    Mapper_018(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_018();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override;

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

private:
    uint8_t prgBank[3] = { 0, 1, 2 };     // $8000, $A000, $C000, 8KB
    uint8_t chrBank[8] = { 0,1,2,3,4,5,6,7 }; // 1KB x 8

    MIRROR mirrorMode = MIRROR::HORIZONTAL;

    uint16_t irqReload = 0x0000;
    uint16_t irqCounter = 0x0000;
    uint8_t irqControl = 0x00;
    bool irqEnable = false;
    bool irqActive = false;

private:
    void SetLowNibble(uint8_t& reg, uint8_t data);
    void SetHighNibble(uint8_t& reg, uint8_t data);
    uint16_t GetIRQMask() const;
};