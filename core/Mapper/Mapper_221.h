#pragma once
#include "Mapper.h"
#include <cstdint>
#include <QString>
class Mapper_221 : public Mapper {
public:
    Mapper_221(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_221();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override;

private:
    uint16_t cmd = 0x0000;
    uint8_t bank = 0x00;

    MIRROR mirrorMode = MIRROR::HORIZONTAL;

    uint32_t prgBank8000 = 0;
    uint32_t prgBankC000 = 0;

    void Sync();
};