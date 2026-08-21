#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_003 : public Mapper {
public:
    Mapper_003(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_003();
    std::string GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;

private:
    uint8_t nCHRBankSelect = 0; // Biến nhớ xem đang cắm cuộn băng hình (CHR) số mấy
};