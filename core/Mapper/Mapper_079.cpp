#include "Mapper_079.h"

Mapper_079::Mapper_079(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_079::~Mapper_079() {}

void Mapper_079::reset()
{
    prg_bank = 0;
    chr_bank = 0;
}

bool Mapper_079::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000)
    {
        // Phải chia đôi nPRGBanks để ra số lượng bank 32KB thực tế
        uint32_t prg_32k_banks = (nPRGBanks / 2) > 0 ? (nPRGBanks / 2) : 1;
        uint32_t bank = prg_bank % prg_32k_banks;

        // Tránh lỗi văng mảng nếu ROM băng game chỉ có 16KB
        uint32_t offset = addr & 0x7FFF;
        if (nPRGBanks == 1) offset = addr & 0x3FFF;

        mapped_addr = bank * 0x8000 + offset;
        return true;
    }
    return false;
}

bool Mapper_079::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    // Mặt nạ chuẩn xác nhất (0xE100) để bắt trúng thanh ghi của Mapper 79 Sachen
    if ((addr & 0xE100) == 0x4100)
    {
        uint8_t prg_32k_banks = (nPRGBanks / 2) > 0 ? (nPRGBanks / 2) : 1;
        uint8_t totalChr = nCHRBanks ? nCHRBanks : 1;

        prg_bank = ((data >> 3) & 0x01) % prg_32k_banks;
        chr_bank = (data & 0x07) % totalChr;

        // Vẫn phải trả về false để bảo vệ ROM gốc không bị giả lập ghi đè
        return false;
    }
    return false;
}

bool Mapper_079::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        uint32_t chrBankSize = 0x2000; // 8KB

        uint32_t bank = chr_bank % (nCHRBanks ? nCHRBanks : 1);

        mapped_addr = bank * chrBankSize + addr;
        return true;
    }

    return false;
}

bool Mapper_079::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    // CHR RAM support
    if (addr <= 0x1FFF && nCHRBanks == 0)
    {
        mapped_addr = addr;
        return true;
    }

    return false;
}

std::string Mapper_079::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 079 DEBUG =====\n\n";

    s += "PRG Bank: " + std::to_string(prg_bank) + " / " + std::to_string(nPRGBanks) + "\n";
    s += "CHR Bank: " + std::to_string(chr_bank) + " / " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG Mapping:\n";
    s += "$8000-$FFFF -> 32KB PRG switchable\n";

    s += "\nCHR Mapping:\n";
    s += "$0000-$1FFF -> 8KB CHR switchable\n";

    s += "\nRegister:\n";
    s += "$4100 (Sachen-style)\n";
    s += "bit 0-2: CHR bank\n";
    s += "bit 3  : PRG bank\n";

    return s;
}