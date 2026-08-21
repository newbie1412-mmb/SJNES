#pragma once
#include <cstdint>
#include "Mapper.h"

class Mapper_068 : public Mapper
{
public:
    Mapper_068(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    MIRROR mirror() override { return HARDWARE; } // tự quản lý

    void clock() override
    {
        if (licensingTimer > 0)
            licensingTimer--;
    }
    
private:
    // CHR regs: $8000-$B000 (4 banks x 2KB)
    uint8_t chrRegs[4] = {};

    // Nametable regs: $C000, $D000
    uint8_t ntRegs[2] = {};          // index vào CHR ROM (đã OR 0x80)
    bool    useChrForNametables = false;

    // Mirroring từ $E000 (bits 0-1)
    uint8_t mirrorMode = 0;          // 0=Vertical, 1=Horizontal, 2=ScreenA, 3=ScreenB

    // PRG: $F000
    uint8_t prgBank = 0;             // bank slot 0 ($8000-$BFFF)
    bool    prgRamEnabled = false;

    // External ROM (Afterburner II...)
    bool    usingExternalRom = false;
    uint8_t externalPage = 0;

    // Licensing timer
    uint32_t licensingTimer = 0;
    static constexpr uint32_t LICENSE_CYCLES = 1024 * 105;

    // Helper: resolve nametable slot i -> CHR offset hoặc VRAM
    // Trả về mapped_addr vào vCHRMemory, hoặc 0x80000000 nếu dùng VRAM
    uint32_t ResolveNametable(uint16_t addr);
};