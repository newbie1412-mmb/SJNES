#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_009 : public Mapper {
public:
    Mapper_009(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_009();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    std::string GetDebugInfo() override;
    void reset() override;

    // Khai báo hàm mirror() để khớp với file .cpp
    MIRROR mirror() override;

private:
    // Toàn bộ "nội tạng" của Mapper 009 (MMC2)
    uint8_t nPRGBank = 0;

    uint8_t nCHRBank0_FD8 = 0;
    uint8_t nCHRBank0_FE8 = 0;
    uint8_t nCHRBank1_FD8 = 0;
    uint8_t nCHRBank1_FE8 = 0;

    uint8_t nLatch0 = 0;
    uint8_t nLatch1 = 0;

    MIRROR mirromode = HORIZONTAL;
};