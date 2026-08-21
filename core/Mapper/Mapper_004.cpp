#include "Mapper_004.h"
Mapper_004::Mapper_004(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_004::~Mapper_004() {}

void Mapper_004::reset() {
    nTargetRegister = 0; bPRGBankMode = false; bCHRInversion = false; mirrormode = MIRROR::HORIZONTAL;
    bIRQActive = false; bIRQEnable = false; bIRQUpdate = false;
    nIRQCounter = 0; nIRQLatch = 0;

    for (int i = 0; i < 8; i++)
    {
        pRegister[i] = 0;
        pCHRBank[i] = i * 0x0400;
    }
    pPRGBank[0] = 0 * 0x2000;
    pPRGBank[1] = 1 * 0x2000;
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000;
}

bool Mapper_004::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

// 2. HÀM CHO PPU: BỘ LỌC A12 (TỈ LỆ 3) VÀ ĐỌC HÌNH ẢNH (CHR)
bool Mapper_004::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    // Chia Bank hình ảnh
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}
bool Mapper_004::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (!(addr & 0x0001)) {
            nTargetRegister = data & 0x07;
            bPRGBankMode = (data & 0x40);
            bCHRInversion = (data & 0x80);
        }
        else {
            pRegister[nTargetRegister] = data;
        }
        
        uint32_t num_prg_banks = nPRGBanks * 2;
        uint32_t num_chr_banks = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        auto wrapPRG = [&](uint32_t b) { return (b % num_prg_banks) * 0x2000; };
        auto wrapCHR = [&](uint32_t b) { return (b % num_chr_banks) * 0x0400; };

        // Xếp cửa sổ Hình Ảnh (CHR)
        if (bCHRInversion) {
            pCHRBank[0] = wrapCHR(pRegister[2]);
            pCHRBank[1] = wrapCHR(pRegister[3]);
            pCHRBank[2] = wrapCHR(pRegister[4]);
            pCHRBank[3] = wrapCHR(pRegister[5]);
            pCHRBank[4] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[5] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[6] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[7] = wrapCHR((pRegister[1] & 0xFE) + 1);
        }
        else {
            pCHRBank[0] = wrapCHR(pRegister[0] & 0xFE);
            pCHRBank[1] = wrapCHR((pRegister[0] & 0xFE) + 1);
            pCHRBank[2] = wrapCHR(pRegister[1] & 0xFE);
            pCHRBank[3] = wrapCHR((pRegister[1] & 0xFE) + 1);
            pCHRBank[4] = wrapCHR(pRegister[2]);
            pCHRBank[5] = wrapCHR(pRegister[3]);
            pCHRBank[6] = wrapCHR(pRegister[4]);
            pCHRBank[7] = wrapCHR(pRegister[5]);
        }

        // Xếp cửa sổ Code (PRG)
        if (bPRGBankMode) {
            pPRGBank[2] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[0] = wrapPRG(num_prg_banks - 2);
        }
        else {
            pPRGBank[0] = wrapPRG(pRegister[6] & 0x3F);
            pPRGBank[2] = wrapPRG(num_prg_banks - 2);
        }
        pPRGBank[1] = wrapPRG(pRegister[7] & 0x3F);
        pPRGBank[3] = wrapPRG(num_prg_banks - 1);

        return false;
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (!(addr & 0x0001)) {
            if (data & 0x01) mirrormode = MIRROR::HORIZONTAL;
            else mirrormode = MIRROR::VERTICAL;
        }
        else {
        }
        return false;
    }
    else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (!(addr & 0x0001)) {
            nIRQLatch = data;
        }
        else {
            bIRQUpdate = true;
        }
        return false;
    }
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        if (!(addr & 0x0001)) {
            bIRQEnable = false;
            bIRQActive = false; 
        }
        else {
            bIRQEnable = true;
        }
        return false;
    }
    return false;
}

bool Mapper_004::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x03FF) { mapped_addr = pCHRBank[0] + (addr & 0x03FF); return true; }
    if (addr >= 0x0400 && addr <= 0x07FF) { mapped_addr = pCHRBank[1] + (addr & 0x03FF); return true; }
    if (addr >= 0x0800 && addr <= 0x0BFF) { mapped_addr = pCHRBank[2] + (addr & 0x03FF); return true; }
    if (addr >= 0x0C00 && addr <= 0x0FFF) { mapped_addr = pCHRBank[3] + (addr & 0x03FF); return true; }
    if (addr >= 0x1000 && addr <= 0x13FF) { mapped_addr = pCHRBank[4] + (addr & 0x03FF); return true; }
    if (addr >= 0x1400 && addr <= 0x17FF) { mapped_addr = pCHRBank[5] + (addr & 0x03FF); return true; }
    if (addr >= 0x1800 && addr <= 0x1BFF) { mapped_addr = pCHRBank[6] + (addr & 0x03FF); return true; }
    if (addr >= 0x1C00 && addr <= 0x1FFF) { mapped_addr = pCHRBank[7] + (addr & 0x03FF); return true; }
    return false;
}
// MMC3 IRQ must be held as an IRQ source/line.
// Do not discard IRQ just because I flag is currently set,
// otherwise split-screen HUD may jitter in games like "Contra Force".
// lỗi irq có thể do CPU sai IRQ (mất 3 tháng để tìm ra lỗi)
void Mapper_004::ClockA12()
{
    if (nIRQCounter == 0 || bIRQUpdate)
    {
        nIRQCounter = nIRQLatch;
        bIRQUpdate = false;
    }
    else
    {
        nIRQCounter--;
    }

    if (nIRQCounter == 0 && bIRQEnable)
    {
        bIRQActive = true;
    }
}
bool Mapper_004::irqState() { return bIRQActive; }
void Mapper_004::irqClear() { bIRQActive = false; }

MIRROR Mapper_004::mirror() {
    return mirrormode;
}

std::string Mapper_004::GetDebugInfo()
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

    s += "===== MAPPER 004 - MMC3 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(nCHRBanks == 0 ? 8 : nCHRBanks * 8) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrormode) + "\n";

    s += "\nTHANH GHI BANK SELECT:\n";
    s += "Target Register R : " + std::to_string(nTargetRegister) + "\n";
    s += std::string("PRG Bank Mode     : ") + (bPRGBankMode ? "1 - co dinh bank ap chot tai $8000" : "0 - co dinh bank ap chot tai $C000") + "\n";
    s += std::string("CHR Inversion     : ") + (bCHRInversion ? "BAT - dao vung CHR $0000/$1000" : "TAT") + "\n";

    s += "\n8 THANH GHI R0-R7:\n";
    for (int i = 0; i < 8; i++)
    {
        s += "R" + std::to_string(i) + " = " + std::to_string(pRegister[i]) + "\n";
    }

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$9FFF : offset ROM = 0x" + HexStr(pPRGBank[0], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[0] / 0x2000) + "\n";

    s += "$A000-$BFFF : offset ROM = 0x" + HexStr(pPRGBank[1], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[1] / 0x2000) + "\n";

    s += "$C000-$DFFF : offset ROM = 0x" + HexStr(pPRGBank[2], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[2] / 0x2000) + "\n";

    s += "$E000-$FFFF : offset ROM = 0x" + HexStr(pPRGBank[3], 6) +
        " | PRG bank 8KB = " + std::to_string(pPRGBank[3] / 0x2000) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";

    if (nCHRBanks == 0)
    {
        s += "Game dung CHR RAM. Cac offset duoi day la offset trong CHR RAM.\n";
    }

    const char* chrRanges[8] = {
        "$0000-$03FF",
        "$0400-$07FF",
        "$0800-$0BFF",
        "$0C00-$0FFF",
        "$1000-$13FF",
        "$1400-$17FF",
        "$1800-$1BFF",
        "$1C00-$1FFF"
    };

    for (int i = 0; i < 8; i++)
    {
        s += std::string(chrRanges[i]) + " : offset CHR = 0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB = " + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }

    s += "\nTHONG TIN IRQ MMC3:\n";
    s += std::string("IRQ Enable        : ") + (bIRQEnable ? "BAT" : "TAT") + "\n";
    s += std::string("IRQ Active        : ") + (bIRQActive ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Reload/Update : ") + (bIRQUpdate ? "CO" : "KHONG") + "\n";
    s += "IRQ Counter       : " + std::to_string(nIRQCounter) + "\n";
    s += "IRQ Latch         : " + std::to_string(nIRQLatch) + "\n";

    s += "\nGIAI THICH NHANH:\n";
    s += "MMC3 dung R0-R5 de chon CHR bank va R6-R7 de chon PRG bank.\n";
    s += "PRG bank co kich thuoc 8KB, nen offset ROM thuong nhay theo 0x2000.\n";
    s += "CHR bank co kich thuoc 1KB, nen offset CHR thuong nhay theo 0x0400.\n";
    s += "IRQ MMC3 thuong dung de chia man hinh, vi du HUD co dinh va nen cuon rieng.\n";

    return s;
}