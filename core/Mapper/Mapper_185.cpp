#include "Mapper_185.h"

Mapper_185::Mapper_185(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
Mapper_185::~Mapper_185() {}

void Mapper_185::reset() {
    // Boot với CHR MỞ — game đọc CHR thật để verify copy protection
    // B-Wings Japan dùng key 0x21 (low nibble != 0 → mở)
    nDummyEnable = 0x21;
}

bool Mapper_185::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_185::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        nDummyEnable = data; // Ghi chép lại lệnh game vừa gửi
    }
    return false;
}

bool Mapper_185::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        mapped_addr = addr & 0x1FFF;
        return true;  // Luôn mở, bỏ qua copy protection
    }
    return false;
}

bool Mapper_185::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) { return false; }
std::string Mapper_185::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 185 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay thuong lien quan toi CHR/copy protection.\n";
    s += "Ban hien tai luon mo CHR de game doc CHR that.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG MAPPING:\n";
    if (nPRGBanks > 1)
    {
        s += "$8000-$FFFF : PRG 32KB co dinh\n";
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : PRG bank 1 | offset ROM = 0x004000\n";
    }
    else
    {
        s += "$8000-$BFFF : PRG bank 0 | offset ROM = 0x000000\n";
        s += "$C000-$FFFF : mirror PRG bank 0 | offset ROM = 0x000000\n";
    }

    s += "\nCHR MAPPING:\n";
    s += "$0000-$1FFF : CHR ROM/RAM map thang, luon mo\n";

    s += "\nTHANH GHI PROTECTION:\n";
    s += "nDummyEnable : 0x" + HexStr(nDummyEnable, 2) + "\n";

    s += "\nGHI CHU:\n";
    s += "Mot so game dung mapper nay de kiem tra CHR. Neu khoa CHR sai co the man hinh den hoac crash.\n";
    s += "Code hien tai bo qua khoa va luon cho doc CHR that.\n";

    return s;
}