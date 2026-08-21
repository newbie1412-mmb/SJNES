#pragma once
#include "Mapper.h"

class Mapper_015 : public Mapper
{
public:
    Mapper_015(uint8_t prgBanks, uint8_t chrBanks);
    void reset() override;
public:
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    bool IsHorizontalMirror() const { return mirrorHorizontal; }

private:
    uint8_t prgBank = 0;
    uint8_t mode = 0;
    bool prgA13 = false;
    bool mirrorHorizontal = false;

private:
    uint32_t Map16K(uint32_t bank, uint16_t addr) const;
    uint32_t Map8K(uint32_t bank, uint16_t addr) const;
};