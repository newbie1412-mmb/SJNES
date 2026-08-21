#include "Mapper_001.h"

Mapper_001::Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {

    reset(); 
}

Mapper_001::~Mapper_001() {}

void Mapper_001::reset() { 
    nControlRegister = 0x1C; // Mode mặc định: PRG 16KB cố định ở 0xC000
    nLoadRegister = 0x00;
    nLoadRegisterCount = 0x00;
    nCHRBankSelect4Lo = 0;
    nCHRBankSelect4Hi = 0;
    nCHRBankSelect8 = 0;
    nPRGBankSelect16Lo = 0;
    nPRGBankSelect16Hi = 0;
    nPRGBankSelect32 = 0;
    pPRGBank[0] = 0;
    pPRGBank[1] = (nPRGBanks - 1) * 0x4000; // Bank cuối cùng luôn nằm ở 0xC000
    pCHRBank[0] = 0;
    pCHRBank[1] = 0x1000;
}

bool Mapper_001::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // NẾU BIT 7 LÀ 1 -> Reset bộ ghi dịch khẩn cấp!
        if (data & 0x80) {
            nLoadRegister = 0x00;
            nLoadRegisterCount = 0;
            nControlRegister = nControlRegister | 0x0C;
        }
        else {
            // Đút từng đồng xu (bit) vào túi
            nLoadRegister >>= 1;
            nLoadRegister |= (data & 0x01) << 4;
            nLoadRegisterCount++;

            // Đã đủ 5 đồng xu -> CHỐT ĐƠN VÀ LẬT BANK!
            if (nLoadRegisterCount == 5) {
                uint8_t nTargetRegister = (addr >> 13) & 0x03;

                if (nTargetRegister == 0) { // 0x8000 - 0x9FFF
                    nControlRegister = nLoadRegister & 0x1F;
                    switch (nControlRegister & 0x03) {
                    case 0: currentMirror = MIRROR::ONESCREEN_LO; break;
                    case 1: currentMirror = MIRROR::ONESCREEN_HI; break;
                    case 2: currentMirror = MIRROR::VERTICAL;     break;
                    case 3: currentMirror = MIRROR::HORIZONTAL;   break;
                    }
                }
                else if (nTargetRegister == 1) { // 0xA000 - 0xBFFF
                    nCHRBankSelect4Lo = nLoadRegister & 0x1F;
                }
                else if (nTargetRegister == 2) { // 0xC000 - 0xDFFF
                    nCHRBankSelect4Hi = nLoadRegister & 0x1F;
                }
                else if (nTargetRegister == 3) { // 0xE000 - 0xFFFF
                    nPRGBankSelect16Lo = nLoadRegister & 0x0F;
                }

                if (nControlRegister & 0x10) {
                    pCHRBank[0] = nCHRBankSelect4Lo * 0x1000;
                    pCHRBank[1] = nCHRBankSelect4Hi * 0x1000;
                }
                else {
                    pCHRBank[0] = (nCHRBankSelect4Lo & 0xFE) * 0x1000;
                    pCHRBank[1] = pCHRBank[0] + 0x1000;
                }

                uint8_t nPRGMode = (nControlRegister >> 2) & 0x03;
                if (nPRGMode == 0 || nPRGMode == 1) {
                    pPRGBank[0] = (nPRGBankSelect16Lo & 0x0E) * 0x4000;
                    pPRGBank[1] = pPRGBank[0] + 0x4000;
                }
                else if (nPRGMode == 2) {
                    pPRGBank[0] = 0;
                    pPRGBank[1] = nPRGBankSelect16Lo * 0x4000;
                }
                else if (nPRGMode == 3) {
                    pPRGBank[0] = nPRGBankSelect16Lo * 0x4000;
                    pPRGBank[1] = (nPRGBanks - 1) * 0x4000;
                }

                nLoadRegister = 0x00;
                nLoadRegisterCount = 0;
            }
        }
        return false;
    }
    return false;
}

bool Mapper_001::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mapped_addr = pPRGBank[0] + (addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xFFFF) {
        mapped_addr = pPRGBank[1] + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_001::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
        else {
            if (addr >= 0x0000 && addr <= 0x0FFF) {
                mapped_addr = pCHRBank[0] + (addr & 0x0FFF);
                return true;
            }
            if (addr >= 0x1000 && addr <= 0x1FFF) {
                mapped_addr = pCHRBank[1] + (addr & 0x0FFF);
                return true;
            }
        }
    }
    return false;
}

bool Mapper_001::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) {
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

std::string Mapper_001::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        default:                   return "Hardware / Khong ro";
        }
        };

    uint8_t chrMode = (nControlRegister >> 4) & 0x01;
    uint8_t prgMode = (nControlRegister >> 2) & 0x03;
    uint8_t mirrorMode = nControlRegister & 0x03;

    s += "===== MAPPER 001 - MMC1 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "Kieu mirroring    : " + mirrorToString(currentMirror) + "\n";

    s += "\nTHANH GHI DIEU KHIEN MMC1:\n";
    s += "Control Register  : 0x" + HexStr(nControlRegister, 2) + "\n";
    s += "Mirror bits       : " + std::to_string(mirrorMode) + "\n";
    s += "PRG mode          : " + std::to_string(prgMode) + " - ";

    if (prgMode == 0 || prgMode == 1)
        s += "32KB switch tai $8000-$FFFF\n";
    else if (prgMode == 2)
        s += "Co dinh bank dau tai $8000, doi bank tai $C000\n";
    else
        s += "Doi bank tai $8000, co dinh bank cuoi tai $C000\n";

    s += "CHR mode          : " + std::to_string(chrMode) + " - ";
    s += (chrMode ? "2 bank CHR 4KB rieng\n" : "1 bank CHR 8KB\n");

    s += "\nBO GHI DICH MMC1:\n";
    s += "Load Register     : 0x" + HexStr(nLoadRegister, 2) + "\n";
    s += "So bit da nap     : " + std::to_string(nLoadRegisterCount) + " / 5\n";

    s += "\nTHANH GHI CHON BANK:\n";
    s += "CHR 4KB Low       : " + std::to_string(nCHRBankSelect4Lo) + "\n";
    s += "CHR 4KB High      : " + std::to_string(nCHRBankSelect4Hi) + "\n";
    s += "CHR 8KB           : " + std::to_string(nCHRBankSelect8) + "\n";
    s += "PRG 16KB Low      : " + std::to_string(nPRGBankSelect16Lo) + "\n";
    s += "PRG 16KB High     : " + std::to_string(nPRGBankSelect16Hi) + "\n";
    s += "PRG 32KB          : " + std::to_string(nPRGBankSelect32) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$BFFF : offset ROM = 0x" + HexStr(pPRGBank[0], 6) +
        " | PRG bank 16KB = " + std::to_string(pPRGBank[0] / 0x4000) + "\n";

    s += "$C000-$FFFF : offset ROM = 0x" + HexStr(pPRGBank[1], 6) +
        " | PRG bank 16KB = " + std::to_string(pPRGBank[1] / 0x4000) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "$0000-$1FFF : CHR RAM\n";
    }
    else
    {
        s += "$0000-$0FFF : offset CHR = 0x" + HexStr(pCHRBank[0], 6) +
            " | CHR bank 4KB = " + std::to_string(pCHRBank[0] / 0x1000) + "\n";

        s += "$1000-$1FFF : offset CHR = 0x" + HexStr(pCHRBank[1], 6) +
            " | CHR bank 4KB = " + std::to_string(pCHRBank[1] / 0x1000) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "MMC1 nap tung bit vao shift register. Khi du 5 bit, mapper moi chot vao thanh ghi dich.\n";
    s += "Vi vay Load Register va so bit da nap co the thay doi rat nhanh khi game dang chay.\n";

    return s;
}