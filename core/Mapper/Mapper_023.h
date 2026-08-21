#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_023 : public Mapper {
public:
    Mapper_023(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_023() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    std::string GetDebugInfo() override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override;

private:
    MIRROR mirrormode = MIRROR::VERTICAL;

    // Cửa sổ bộ nhớ (Giống MMC3)
    uint32_t pPRGBank[4];  // 4 ô 8KB cho Code
    uint32_t pCHRBank[8];  // 8 ô 1KB cho Đồ họa

    // Mảng hứng dữ liệu "Cắt nửa vầng trăng" của VRC2
    uint8_t nCHRBank_Lo[8]; // Hứng 4 bit thấp
    uint8_t nCHRBank_Hi[8]; // Hứng 4 bit cao
};