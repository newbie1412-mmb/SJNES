#include "Mapper_066.h"

Mapper_066::Mapper_066(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_066::~Mapper_066()
{}

void Mapper_066::reset()
{
    prgBankSelect = 0;
    chrBankSelect = 0;
}

bool Mapper_066::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        uint32_t prg32Count = nPRGBanks / 2;

        if (prg32Count == 0)
            prg32Count = 1;

        mapped_addr = (prgBankSelect % prg32Count) * 0x8000 + (addr & 0x7FFF);
        return true;
    }

    return false;
}

bool Mapper_066::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        // Mapper 66 / GxROM:
        // bit 4-5: chọn PRG bank 32KB
        // bit 0-1: chọn CHR bank 8KB
        prgBankSelect = (data >> 4) & 0x03;
        chrBankSelect = data & 0x03;

        return false;
    }

    return false;
}

bool Mapper_066::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            // Trường hợp CHR RAM hiếm hoặc homebrew
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        mapped_addr = (chrBankSelect % nCHRBanks) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_066::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }
    }

    return false;
}

std::string Mapper_066::GetDebugInfo()
{
    std::string s;

    uint32_t prg32Count = nPRGBanks / 2;
    if (prg32Count == 0)
        prg32Count = 1;

    s += "===== MAPPER 066 - GXROM / GNROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay dung PRG bank 32KB va CHR bank 8KB.\n";
    s += "CPU ghi vao $8000-$FFFF de chon ca PRG va CHR bank.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 32KB : " + std::to_string(prg32Count) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nTHANH GHI MAPPER:\n";
    s += "PRG bank select   : " + std::to_string(prgBankSelect) + "\n";
    s += "CHR bank select   : " + std::to_string(chrBankSelect) + "\n";
    s += "Bit 4-5 cua data  : chon PRG bank 32KB\n";
    s += "Bit 0-1 cua data  : chon CHR bank 8KB\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$FFFF : PRG bank 32KB = " + std::to_string(prgBankSelect % prg32Count) +
        " | offset ROM = 0x" + HexStr((prgBankSelect % prg32Count) * 0x8000, 6) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";
    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR bank 8KB = " + std::to_string(chrBankSelect % nCHRBanks) +
            " | offset CHR = 0x" + HexStr((chrBankSelect % nCHRBanks) * 0x2000, 6) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "Offset PRG nhay theo 0x8000 vi moi PRG bank la 32KB.\n";
    s += "Offset CHR nhay theo 0x2000 vi moi CHR bank la 8KB.\n";

    return s;
}