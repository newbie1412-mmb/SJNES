#pragma once
#include "Mapper.h"
#include <QString>
class Mapper_087 : public Mapper {
public:
    Mapper_087(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_087();
    std::string GetDebugInfo() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;    void reset() override;

private:
    // Biến lưu trữ số thứ tự của cuộn hình (CHR Bank) đang được chọn
    uint8_t chrBankSelect = 0;
};