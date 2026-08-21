#include "Mapper_015.h"

Mapper_015::Mapper_015(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

void Mapper_015::reset()
{
    prgBank = 0;
    mode = 0;
    prgA13 = false;
    mirrorHorizontal = false;
}

uint32_t Mapper_015::Map16K(uint32_t bank, uint16_t addr) const
{
    uint32_t totalBanks = nPRGBanks == 0 ? 1 : nPRGBanks;
    bank %= totalBanks;

    return bank * 0x4000 + (addr & 0x3FFF);
}

uint32_t Mapper_015::Map8K(uint32_t bank, uint16_t addr) const
{
    uint32_t totalBanks = nPRGBanks == 0 ? 2 : uint32_t(nPRGBanks) * 2;
    bank %= totalBanks;

    return bank * 0x2000 + (addr & 0x1FFF);
}

bool Mapper_015::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x8000)
        return false;

    uint32_t bank = prgBank & 0x3F;

    switch (mode)
    {
    case 0:
        // NROM-256: 32KB, CPU A14 chọn nửa dưới/nửa trên
        // $8000-$BFFF = even bank, $C000-$FFFF = odd bank
        bank = (bank & 0x3E) | ((addr >> 14) & 0x01);
        mapped_addr = Map16K(bank, addr);
        return true;

    case 1:
        // UNROM-like:
        // $8000-$BFFF = bank chọn
        // $C000-$FFFF = bank cố định theo OR 7
        if (addr < 0xC000)
            bank = bank;
        else
            bank = bank | 0x07;

        mapped_addr = Map16K(bank, addr);
        return true;

    case 2:
        // NROM-64: 8KB, PRG A13 lấy từ bit 7 data
        // Mirror cùng 8KB qua toàn vùng $8000-$FFFF
        bank = (bank << 1) | (prgA13 ? 1 : 0);
        mapped_addr = Map8K(bank, addr);
        return true;

    case 3:
        // NROM-128: 16KB mirror ở $8000-$FFFF
        mapped_addr = Map16K(bank, addr);
        return true;
    }

    mapped_addr = 0;
    return true;
}

bool Mapper_015::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    if (addr < 0x8000)
        return false;

    // Address bit SS chọn mode
    mode = addr & 0x03;

    // D0-D5 = PRG bank bits
    prgBank = data & 0x3F;

    // D6 = mirroring: 0 vertical, 1 horizontal
    mirrorHorizontal = (data & 0x40) != 0;

    // D7 = PRG A13 khi mode = 2
    prgA13 = (data & 0x80) != 0;

    mapped_addr = 0;
    return true;
}

bool Mapper_015::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        // Mapper 15 dùng CHR-RAM 8KB, không bank
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    return false;
}

bool Mapper_015::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        // Với ROM hack như Bio Hazard/Resident Evil, cứ cho ghi CHR-RAM
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    return false;
}
