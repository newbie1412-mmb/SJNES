#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_185 : public Mapper {
public:
    Mapper_185(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_185();
    std::string GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;

private:
    uint8_t nDummyEnable = 0x00; // Lưu lại lệnh của game
};