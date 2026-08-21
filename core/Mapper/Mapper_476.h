#pragma once
#include "Mapper.h"
#include <QString>
#include <vector>

class Mapper_476 : public Mapper {
public:
    Mapper_476(uint32_t prgBanks, uint32_t chrBanks);
    ~Mapper_476();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    void reset() override;
    std::string GetDebugInfo() override;

    uint8_t ReadFrameNametable(uint16_t offset) const {
        size_t base = (size_t)nCHRBankSelect * 960;
        size_t idx = base + offset;
        if (idx < vNametableData.size())
            return vNametableData[idx];
        return 0;
    }

    void SetNametableData(std::vector<uint8_t> data) {
        vNametableData = std::move(data);
    }

private:
    uint8_t nPRGBankSelect = 0x00;
    uint16_t nCHRBankSelect = 0x0000;
    std::vector<uint8_t> vNametableData;
};