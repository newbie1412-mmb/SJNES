#pragma once
#include "Mapper.h"
#include <QString>

// Mapper 021 gộp chung 2 board Konami khác nhau, khác nhau ở việc
// CPU address bit nào nối vào chân A0/A1 của chip (quyết định latch nào
// được chọn trong mỗi block $8000/$9000/$A000/...):
//
//   VRC4a : A0 = CPU bit 1, A1 = CPU bit 2   (vd: Wai Wai World 2)
//   VRC4c : A0 = CPU bit 6, A1 = CPU bit 7   (vd: Ganbare Goemon Gaiden 2)
//
// KHÔNG thể tự detect được variant nào chỉ từ iNES header (cả 2 đều mapper 21).
// Phải set cứng theo game đang chạy.
enum class VRC4Variant {
    VRC4a, // Wai Wai World 2
    VRC4c  // Ganbare Goemon Gaiden 2
};

class Mapper_021 : public Mapper {
public:
    Mapper_021(uint8_t prgBanks, uint8_t chrBanks, VRC4Variant variant = VRC4Variant::VRC4a);
    ~Mapper_021();

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    MIRROR mirror() override;
    void reset() override;
    std::string GetDebugInfo() override;
    bool irqState() override;
    void irqClear() override;
    void irqStep();

private:
    VRC4Variant nVariant;

    // Physical addresses (đã tính sẵn, PPU/CPU dùng trực tiếp)
    uint32_t pPRGBank[4];    // 4 x 8KB = 32KB PRG
    uint32_t pCHRBank[8];    // 8 x 1KB = 8KB CHR

    // Bank select registers
    uint8_t  nPRGBankSelect[2];   // Chọn bank cho slot 0 và 1
    uint16_t nCHRBankSelect[8];   // 9-bit
    int     mirrormode;
    uint8_t nPRGSwapMode;

    // IRQ
    int     nIRQPrescaler;
    uint8_t nIRQReload;
    uint8_t nIRQCounter;
    bool    bIRQEnable;
    bool    bIRQEnableAfterAck;
    bool    bIRQActive;
};