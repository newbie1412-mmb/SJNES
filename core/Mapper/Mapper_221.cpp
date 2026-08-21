#include "Mapper_221.h"

Mapper_221::Mapper_221(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_221::~Mapper_221() {}

void Mapper_221::reset()
{
    cmd = 0x0000;
    bank = 0x00;
    Sync();
}

void Mapper_221::Sync()
{
    // Mapper 221 / NTDEC N625092:
    // cmd bit 0 = mirroring control
    // FCEUX dùng setmirror((cmd & 1) ^ 1)
    // Trong enum của bạn: HORIZONTAL / VERTICAL
    mirrorMode = (cmd & 0x0001) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;

    const uint32_t prg16Count = nPRGBanks;
    if (prg16Count == 0)
    {
        prgBank8000 = 0;
        prgBankC000 = 0;
        return;
    }

    // Base chọn nhóm PRG lớn
    uint32_t base = (cmd & 0x00FC) >> 2;

    if (cmd & 0x0002)
    {
        // Mode 16KB switch
        if (cmd & 0x0100)
        {
            // $8000 = base | bank
            // $C000 = base | 7
            prgBank8000 = (base | bank) % prg16Count;
            prgBankC000 = (base | 0x07) % prg16Count;
        }
        else
        {
            // $8000 = base | even bank
            // $C000 = base | odd bank
            prgBank8000 = (base | (bank & 0x06)) % prg16Count;
            prgBankC000 = (base | ((bank & 0x06) | 0x01)) % prg16Count;
        }
    }
    else
    {
        // Mode 32KB nhưng map bằng 2 nửa 16KB giống nhau theo công thức board
        prgBank8000 = (base | bank) % prg16Count;
        prgBankC000 = (base | bank) % prg16Count;
    }
}

bool Mapper_221::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        mapped_addr = prgBank8000 * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF)
    {
        mapped_addr = prgBankC000 * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    return false;
}

bool Mapper_221::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // Mapper này chọn bank bằng ĐỊA CHỈ ghi, data thường không quan trọng
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        cmd = addr;
        Sync();
        return false;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF)
    {
        bank = addr & 0x0007;
        Sync();
        return false;
    }

    return false;
}

bool Mapper_221::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        // ROM này header CHR = 0, tức dùng CHR RAM 8KB
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    return false;
}

bool Mapper_221::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        mapped_addr = addr & 0x1FFF;

        // Chỉ cho ghi nếu là CHR RAM
        return nCHRBanks == 0;
    }

    return false;
}

MIRROR Mapper_221::mirror()
{
    return mirrorMode;
}

std::string Mapper_221::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Khong ro";
        }
        };

    uint32_t base = (cmd & 0x00FC) >> 2;

    s += "===== MAPPER 221 - NTDEC N625092 / MULTICART =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay thuong gap o bang multicart.\n";
    s += "Bank duoc chon chu yeu bang dia chi ghi, data thuong khong quan trong.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrorMode) + "\n";

    s += "\nTHANH GHI MULTICART:\n";
    s += "cmd  : 0x" + HexStr(cmd, 4) + "\n";
    s += "bank : " + std::to_string(bank) + "\n";
    s += "base : " + std::to_string(base) + "\n";

    s += "\nGIAI MA CMD:\n";
    s += std::string("cmd bit 0 - Mirroring      : ") + (cmd & 0x0001 ? "Horizontal" : "Vertical") + "\n";
    s += std::string("cmd bit 1 - Mode           : ") + (cmd & 0x0002 ? "16KB switch" : "32KB style") + "\n";
    s += std::string("cmd bit 8 - 16KB sub-mode  : ") + (cmd & 0x0100 ? "$8000=base|bank, $C000=base|7" : "$8000 even, $C000 odd") + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$BFFF : PRG bank 16KB = " + std::to_string(prgBank8000) +
        " | offset ROM = 0x" + HexStr(prgBank8000 * 0x4000, 6) + "\n";

    s += "$C000-$FFFF : PRG bank 16KB = " + std::to_string(prgBankC000) +
        " | offset ROM = 0x" + HexStr(prgBankC000 * 0x4000, 6) + "\n";

    s += "\nCHR MAPPING:\n";
    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM 8KB, PPU co the ghi\n";
    }
    else
    {
        s += "$0000-$1FFF : CHR ROM/RAM map thang\n";
    }

    s += "\nGHI CHU:\n";
    s += "Ghi $8000-$BFFF cap nhat cmd.\n";
    s += "Ghi $C000-$FFFF cap nhat bank = addr & 7.\n";
    s += "Sau moi lan ghi, Sync() se tinh lai PRG bank va mirroring.\n";

    return s;
}