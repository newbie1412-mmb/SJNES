#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>

class Mapper_066 : public Mapper
{
public:
    Mapper_066(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_066();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;

    std::string GetDebugInfo() override;

private:
    uint8_t prgBankSelect = 0; // PRG bank 32KB
    uint8_t chrBankSelect = 0; // CHR bank 8KB
};