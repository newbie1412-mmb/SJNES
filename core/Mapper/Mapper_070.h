#pragma once

#include "Mapper.h"

class Mapper_070 : public Mapper
{
public:
    Mapper_070(uint8_t prgBanks, uint8_t chrBanks);

public:
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;

    MIRROR mirror() override;

private:
    uint8_t nPRGBank = 0;
    uint8_t nCHRBank = 0;

    MIRROR mirrormode = HORIZONTAL;
};