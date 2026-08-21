#include "Mapper_000.h"
#include <string>

Mapper_000::Mapper_000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

Mapper_000::~Mapper_000() {}

bool Mapper_000::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_000::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

void Mapper_000::reset() {}

std::string Mapper_000::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 000 - NROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay khong co bank switching.\n";
    s += "PRG va CHR duoc map co dinh.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";

    if (nPRGBanks > 1)
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror lai PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, PPU co the ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM bank 0, map thang\n";
    }

    s += "\nMAPPING CPU:\n";
    s += "CPU $8000-$FFFF duoc map bang addr & 0x7FFF neu co 32KB PRG.\n";
    s += "Neu chi co 16KB PRG, dung addr & 0x3FFF de mirror vung $C000-$FFFF.\n";

    s += "\nMAPPING PPU:\n";
    s += "PPU $0000-$1FFF map thang vao CHR ROM/RAM.\n";

    return s;
}