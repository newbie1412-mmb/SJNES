#include "Mapper_003.h"

Mapper_003::Mapper_003(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_003::~Mapper_003() {}

bool Mapper_003::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
       
        if (nPRGBanks == 1) {
            mapped_addr = addr & 0x3FFF;
        }
        else {
            mapped_addr = addr & 0x7FFF;
        }
        return true;
    }
    return false;
}

bool Mapper_003::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // CHÌA KHÓA CỦA MAPPER 3: Ghi vào CPU để đổi cuộn băng hình (CHR)
        // Thường CNROM chỉ dùng 2 bit cuối của data để chọn 4 bank CHR (0-3)
        nCHRBankSelect = data & 0x03;
        if (nCHRBanks > 0) {
            nCHRBankSelect %= nCHRBanks;
        }

        // Vẫn trả về false để báo là KHÔNG ghi đè lên ROM
        return false;
    }
    return false;
}

bool Mapper_003::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        // PPU đòi đọc hình, Mapper sẽ trỏ tới đúng cái Bank CHR đang được chọn
        mapped_addr = (nCHRBankSelect * 0x2000) + addr;
        return true;
    }
    return false;
}

bool Mapper_003::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    // Mapper 3 thường là băng ROM (không cho ghi đè hình)
    return false;
}

void Mapper_003::reset() {
    nCHRBankSelect = 0;
}
std::string Mapper_003::GetDebugInfo()
{
    std::string s;

    s += "===== MAPPER 003 - CNROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "Mapper nay khong doi bank PRG.\n";
    s += "Mapper nay chi doi bank CHR 8KB cho PPU.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";

    if (nPRGBanks == 1)
    {
        s += "$8000-$BFFF : PRG bank 0\n";
        s += "$C000-$FFFF : mirror PRG bank 0\n";
    }
    else
    {
        s += "$8000-$FFFF : PRG 32KB co dinh\n";
        s += "$8000-$BFFF : PRG bank 0\n";
        s += "$C000-$FFFF : PRG bank 1\n";
    }

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : khong co CHR ROM / co the la CHR RAM\n";
    }
    else
    {
        s += "$0000-$1FFF : dang tro toi CHR bank 8KB so " + std::to_string(nCHRBankSelect) + "\n";

        s += "Offset CHR ROM     : 0x" + HexStr(nCHRBankSelect * 0x2000, 6) + "\n";
    }

    s += "\nTHANH GHI MAPPER:\n";
    s += "nCHRBankSelect     : " + std::to_string(nCHRBankSelect) + "\n";

    s += "\nGHI CHU:\n";
    s += "CPU ghi vao vung $8000-$FFFF de chon bank CHR.\n";
    s += "CNROM thuong dung 2 bit thap cua data de chon toi da 4 bank CHR.\n";
    s += "PPU doc $0000-$1FFF se lay du lieu tu CHR bank dang chon.\n";

    return s;
}