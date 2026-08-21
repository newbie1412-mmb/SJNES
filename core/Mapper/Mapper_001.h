#pragma once
#include "Mapper.h"
#include <QString>

class Mapper_001 : public Mapper {
public:
    Mapper_001(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper_001();

    // 4 hàm cơ bản để giao tiếp với CPU và PPU
    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override { return currentMirror; }
    // Bắt buộc phải có hàm reset cho MMC1
    void reset() override;
    std::string GetDebugInfo() override;
private:
    // CÁC THANH GHI CHỌN BANK (BANK SELECT REGISTERS)
    MIRROR currentMirror = MIRROR::HORIZONTAL;
    // Bank hình ảnh (CHR)
    uint8_t nCHRBankSelect4Lo = 0x00;
    uint8_t nCHRBankSelect4Hi = 0x00;
    uint8_t nCHRBankSelect8 = 0x00;
    // Bank code game (PRG)
    uint8_t nPRGBankSelect16Lo = 0x00;
    uint8_t nPRGBankSelect16Hi = 0x00;
    uint8_t nPRGBankSelect32 = 0x00;
    // Cái túi để nhét từng bit một (đủ 5 bit thì chốt đơn)
    uint8_t nLoadRegister = 0x00;
    // Đếm xem CPU đã nhét được mấy bit vào túi rồi
    uint8_t nLoadRegisterCount = 0x00;
    // Khởi tạo mặc định là 0x1C (0b11100) theo chuẩn phần cứng để game không bị crash lúc mới bật
    uint8_t nControlRegister = 0x1C;
    uint32_t pPRGBank[2] = { 0 };
    uint32_t pCHRBank[2] = { 0 };
};