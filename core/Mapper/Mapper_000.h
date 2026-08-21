#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_000 : public Mapper {
public:
    Mapper_000(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_000();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data = 0) override;
    std::string GetDebugInfo() override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
};