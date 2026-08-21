#pragma once
#include "Mapper_021.h"

class Mapper_025 : public Mapper_021 {
public:
    Mapper_025(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_025();

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
};