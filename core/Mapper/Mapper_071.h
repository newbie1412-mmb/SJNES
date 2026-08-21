#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>

class Mapper_071 : public Mapper
{
public:
    Mapper_071(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_071();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override;

    std::string GetDebugInfo() override;

private:
    uint8_t prgBankSelect = 0;
    MIRROR mirrorMode = MIRROR::HARDWARE;
};