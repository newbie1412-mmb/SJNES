#pragma once
#include "Mapper.h"
#include <QString>

class Mapper_075 : public Mapper {
private:
    uint8_t prg_bank[4];
    uint8_t chr_bank[2];
    uint8_t chr_bit_high_0;
    uint8_t chr_bit_high_1;
    uint8_t mirroring_mode;

public:
    Mapper_075(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_075();

    // Khớp chuẩn xác từng tham số theo đúng khuôn Mapper.h của nhóm
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override; // Ghi đè hàm mirror để xử lý cuộn màn hình tự động
    std::string GetDebugInfo() override;
};