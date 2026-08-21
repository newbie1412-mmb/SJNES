#include "Mapper_071.h"
#include <QString>
#include <QChar>

Mapper_071::Mapper_071(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_071::~Mapper_071()
{}

void Mapper_071::reset()
{
    prgBankSelect = 0;
    mirrorMode = MIRROR::HARDWARE;
}

bool Mapper_071::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        mapped_addr = (prgBankSelect % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF)
    {
        mapped_addr = (nPRGBanks - 1) * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    return false;
}

bool Mapper_071::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // Một số board Camerica/Fire Hawk có latch mirroring ở $9000-$9FFF.
    // Nhưng MiG 29 / BF9093 thường không cần one-screen mirroring.
    // Nếu ROM không phải Fire Hawk thì tốt nhất đừng ép mirroring tại đây.
    if (addr >= 0x9000 && addr <= 0x9FFF)
    {
        // Tạm thời KHÔNG đổi mirrorMode để tránh phá game BF9093 như MiG 29.
        // Nếu sau này test Fire Hawk cần thì bật riêng bằng submapper/CRC.
        return false;
    }

    // Camerica mapper 71 phần lớn hành xử giống UNROM:
    // ghi $8000-$FFFF để chọn PRG bank 16KB tại $8000-$BFFF.
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        prgBankSelect = data & 0x0F;
        return false;
    }

    return false;
}

bool Mapper_071::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        // Mapper 071 thường dùng CHR RAM 8KB
        mapped_addr = addr & 0x1FFF;
        return true;
    }

    return false;
}

bool Mapper_071::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        // Cho ghi nếu game dùng CHR RAM.
        // Nếu ROM có CHR ROM thì không cho ghi.
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }
    }

    return false;
}

MIRROR Mapper_071::mirror()
{
    return MIRROR::HARDWARE;
}

std::string Mapper_071::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        case MIRROR::HARDWARE:     return "Theo header ROM";
        default:                   return "Khong ro";
        }
        };

    s += "===== MAPPER 071 - CAMERICA / CODEMASTERS =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay doi PRG bank 16KB tai $8000-$BFFF.\n";
    s += "$C000-$FFFF co dinh o bank cuoi.\n";
    s += "Phan lon game dung CHR RAM 8KB.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrorMode) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$BFFF : PRG bank 16KB = " + std::to_string(prgBankSelect % nPRGBanks) +
        " | offset ROM = 0x" + HexStr((prgBankSelect % nPRGBanks) * 0x4000, 6) + "\n";

    s += "$C000-$FFFF : PRG bank 16KB cuoi = " + std::to_string(nPRGBanks - 1) +
        " | offset ROM = 0x" + HexStr((nPRGBanks - 1) * 0x4000, 6) + "\n";

    s += "\nCHR:\n";
    if (nCHRBanks == 0)
        s += "$0000-$1FFF : CHR RAM 8KB\n";
    else
        s += "$0000-$1FFF : CHR ROM/RAM map thang\n";

    s += "\nTHANH GHI MAPPER:\n";
    s += "PRG bank select : " + std::to_string(prgBankSelect) + "\n";
    s += "$C000-$FFFF: ghi data de chon PRG bank.\n";
    s += "$9000-$9FFF: mot so board dung de chon one-screen mirroring.\n";

    s += "\nGHI CHU:\n";
    s += "Mapper 071 co vai bien the board. Fire Hawk co mirroring dac biet, con da so game dung mirroring co dinh theo header.\n";

    return s;
}