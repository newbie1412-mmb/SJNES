#pragma once

#include "Mapper.h"
#include <QString>

class Mapper_079 : public Mapper
{
private:
    uint8_t prg_bank;
    uint8_t chr_bank;

public:
    Mapper_079(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_079();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;

    std::string GetDebugInfo() override;
};
