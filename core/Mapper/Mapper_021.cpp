#include "Mapper_021.h"
#include <cstdio>
#include "LogBuffer.h"
Mapper_021::Mapper_021(uint8_t prgBanks, uint8_t chrBanks, VRC4Variant variant)
    : Mapper(prgBanks, chrBanks), nVariant(variant) {
    reset();
}

Mapper_021::~Mapper_021() {}

void Mapper_021::reset() {
    nPRGSwapMode = 0;

    // Reset IRQ
    nIRQReload = 0;
    nIRQCounter = 0;
    nIRQPrescaler = 341;
    bIRQEnable = false;
    bIRQEnableAfterAck = false;
    bIRQActive = false;
    mirrormode = 0;
    // Khởi tạo PRG (Chia làm 4 cục, mỗi cục 8KB)
    pPRGBank[0] = 0;
    pPRGBank[1] = 0x2000; // 8KB
    // Hai bank cuối luôn cố định ở cuối ROM lúc khởi động để game không bị crash
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000;

    // Khởi tạo CHR (Chia làm 8 cục, mỗi cục 1KB)
    for (int i = 0; i < 8; i++) {
        nCHRBankSelect[i] = 0;
        pCHRBank[i] = 0;
    }
}

bool Mapper_021::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_021::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // Lấy 2 bit latch (A0chip, A1chip) tùy theo variant wiring của board
        uint16_t a0, a1;
        if (nVariant == VRC4Variant::VRC4a) {
            // VRC4a: chip A0 = CPU bit1, chip A1 = CPU bit2   (Wai Wai World 2)
            a0 = (addr >> 1) & 0x01;
            a1 = (addr >> 2) & 0x01;
        }
        else {
            // VRC4c: chip A0 = CPU bit6, chip A1 = CPU bit7   (Ganbare Goemon Gaiden 2)
            a0 = (addr >> 6) & 0x01;
            a1 = (addr >> 7) & 0x01;
        }
        uint16_t latch = (a1 << 1) | a0;              // 0..3, latch trong block
        uint16_t reg = (addr & 0xF000) | latch;        // Block ($8000/$9000/...) + latch

        switch (reg) {
        case 0x8000: // Thanh ghi Bank 0 (latch 0)
        case 0x8002: // Trên VRC4a/c, latch 0 và 2 đều là PRG bank 0 (theo tài liệu NesDev)
            nPRGBankSelect[0] = data & ((nPRGBanks * 2) - 1);
            break;
        case 0xA000: // Thanh ghi Bank 1 (latch 0)
        case 0xA002:
            nPRGBankSelect[1] = data & ((nPRGBanks * 2) - 1);
            break;
        case 0x9000: // Chế độ lật Bank (latch 0)
        case 0x9002:
            nPRGSwapMode = (data & 0x02) >> 1;
            break;
        case 0x9001: // Mirroring (latch 1)
        case 0x9003: // Mirroring (latch 3) - một số bản VRC4 lặp lại ở cả 2 latch
            switch (data & 0x03) {
            case 0: mirrormode = MIRROR::VERTICAL; break;
            case 1: mirrormode = MIRROR::HORIZONTAL; break;
            case 2: mirrormode = MIRROR::ONESCREEN_LO; break;
            case 3: mirrormode = MIRROR::ONESCREEN_HI; break;
            }
            break;

            // =====================================
            // LẬT BANK CHR (HÌNH ẢNH)
            // =====================================
            // Bank CHR 0
        case 0xB000: nCHRBankSelect[0] = (nCHRBankSelect[0] & 0xF0) | (data & 0x0F); break;
        case 0xB001: nCHRBankSelect[0] = (nCHRBankSelect[0] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 1
        case 0xB002: nCHRBankSelect[1] = (nCHRBankSelect[1] & 0xF0) | (data & 0x0F); break;
        case 0xB003: nCHRBankSelect[1] = (nCHRBankSelect[1] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 2
        case 0xC000: nCHRBankSelect[2] = (nCHRBankSelect[2] & 0xF0) | (data & 0x0F); break;
        case 0xC001: nCHRBankSelect[2] = (nCHRBankSelect[2] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 3
        case 0xC002: nCHRBankSelect[3] = (nCHRBankSelect[3] & 0xF0) | (data & 0x0F); break;
        case 0xC003: nCHRBankSelect[3] = (nCHRBankSelect[3] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 4
        case 0xD000: nCHRBankSelect[4] = (nCHRBankSelect[4] & 0xF0) | (data & 0x0F); break;
        case 0xD001: nCHRBankSelect[4] = (nCHRBankSelect[4] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 5
        case 0xD002: nCHRBankSelect[5] = (nCHRBankSelect[5] & 0xF0) | (data & 0x0F); break;
        case 0xD003: nCHRBankSelect[5] = (nCHRBankSelect[5] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 6
        case 0xE000: nCHRBankSelect[6] = (nCHRBankSelect[6] & 0xF0) | (data & 0x0F); break;
        case 0xE001: nCHRBankSelect[6] = (nCHRBankSelect[6] & 0x0F) | ((data & 0x0F) << 4); break;
            // Bank CHR 7
        case 0xE002: nCHRBankSelect[7] = (nCHRBankSelect[7] & 0xF0) | (data & 0x0F); break;
        case 0xE003: nCHRBankSelect[7] = (nCHRBankSelect[7] & 0x0F) | ((data & 0x0F) << 4); break;

            // =====================================
            // BỘ ĐIỀU KHIỂN NGẮT (IRQ)
            // =====================================
        case 0xF000:   // Low 4 bits reload
            nIRQReload = (nIRQReload & 0xF0) | (data & 0x0F);
            break;
        case 0xF001:   // High 4 bits reload
            nIRQReload = (nIRQReload & 0x0F) | ((data & 0x0F) << 4);
            break;
        case 0xF002:
            bIRQEnableAfterAck = (data & 0x01) != 0;
            bIRQEnable = (data & 0x02) != 0;
            if (bIRQEnable) {
                nIRQCounter = nIRQReload;
            }
            nIRQPrescaler = 341;
            bIRQActive = false;
            break;
        case 0xF003: // Xác nhận IRQ (Acknowledge)
            bIRQActive = false;
            bIRQEnable = bIRQEnableAfterAck;
            break;
        }

        // --- CHỐT ĐỊA CHỈ PRG SAU KHI LẬT ---
        if (nPRGSwapMode == 0) {
            pPRGBank[0] = nPRGBankSelect[0] * 0x2000;
            pPRGBank[1] = nPRGBankSelect[1] * 0x2000;
            pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000;
        }
        else {
            pPRGBank[0] = (nPRGBanks * 2 - 2) * 0x2000;
            pPRGBank[1] = nPRGBankSelect[1] * 0x2000;
            pPRGBank[2] = nPRGBankSelect[0] * 0x2000;
        }

        // --- CHỐT ĐỊA CHỈ CHR SAU KHI LẬT ---
        for (int i = 0; i < 8; i++) {
            pCHRBank[i] = nCHRBankSelect[i] * 0x0400;
        }
        return false;
    }
    return false;
}

void Mapper_021::irqStep() {
    if (!bIRQEnable) return;

    nIRQPrescaler--;
    if (nIRQPrescaler < 0) {
        nIRQPrescaler += 341;

        if (nIRQCounter == 0x00) {
            nIRQCounter = nIRQReload;
            bIRQActive = true;
        }
        else {
            nIRQCounter--;
        }
    }
}
bool Mapper_021::irqState() {
    return bIRQActive;
}

void Mapper_021::irqClear() {
    bIRQActive = false;
}

bool Mapper_021::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        uint8_t bank = addr / 0x0400;
        uint16_t offset = addr & 0x03FF;
        mapped_addr = pCHRBank[bank] + offset;
        return true;
    }
    return false;
}

bool Mapper_021::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x2000) {
        if (nCHRBanks == 0) { // Nếu dùng CHR RAM
            mapped_addr = addr;
            return true;
        }
    }
    return false;
}

MIRROR Mapper_021::mirror() {
    return (MIRROR)mirrormode;
}
std::string Mapper_021::GetDebugInfo()
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

    s += "===== MAPPER 021 - VRC4 (";
    s += (nVariant == VRC4Variant::VRC4a ? "VRC4a" : "VRC4c");
    s += ") =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(nCHRBanks == 0 ? 8 : nCHRBanks * 8) + "\n";
    s += "Mirroring         : " + mirrorToString((MIRROR)mirrormode) + "\n";

    s += "\nCHE DO PRG:\n";
    s += "PRG Swap Mode     : " + std::to_string(nPRGSwapMode) + "\n";
    if (nPRGSwapMode == 0)
    {
        s += "$8000-$9FFF doi bang PRG select 0\n";
        s += "$A000-$BFFF doi bang PRG select 1\n";
        s += "$C000-$DFFF co dinh bank ap chot\n";
    }
    else
    {
        s += "$8000-$9FFF co dinh bank ap chot\n";
        s += "$A000-$BFFF doi bang PRG select 1\n";
        s += "$C000-$DFFF doi bang PRG select 0\n";
    }

    s += "\nTHANH GHI CHON PRG:\n";
    s += "PRG Select 0      : " + std::to_string(nPRGBankSelect[0]) + "\n";
    s += "PRG Select 1      : " + std::to_string(nPRGBankSelect[1]) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    const char* prgRange[4] = {
        "$8000-$9FFF",
        "$A000-$BFFF",
        "$C000-$DFFF",
        "$E000-$FFFF"
    };

    for (int i = 0; i < 4; i++)
    {
        s += std::string(prgRange[i]) + " : offset ROM = 0x" + HexStr(pPRGBank[i], 6) +
            " | PRG bank 8KB = " + std::to_string(pPRGBank[i] / 0x2000) + "\n";
    }

    s += "\nCHR BANK HIEN TAI:\n";
    for (int i = 0; i < 8; i++)
    {
        uint16_t start = i * 0x0400;
        s += "$" + HexStr(start, 4) + "-$" + HexStr(start + 0x03FF, 4) +
            " : select=" + std::to_string(nCHRBankSelect[i]) +
            " | offset CHR=0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB=" + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }

    s += "\nTHONG TIN IRQ VRC4:\n";
    s += std::string("IRQ Enable          : ") + (bIRQEnable ? "BAT" : "TAT") + "\n";
    s += std::string("IRQ Enable After ACK: ") + (bIRQEnableAfterAck ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Active          : ") + (bIRQActive ? "CO" : "KHONG") + "\n";
    s += "IRQ Reload          : " + std::to_string(nIRQReload) + "\n";
    s += "IRQ Counter         : " + std::to_string(nIRQCounter) + "\n";
    s += "IRQ Prescaler       : " + std::to_string(nIRQPrescaler) + "\n";

    s += "\nGHI CHU:\n";
    s += "Mapper 021 la VRC4. PRG bank 8KB, CHR bank 1KB.\n";
    s += "IRQ dung prescaler 341 va counter 8-bit de tao ngat scanline.\n";

    return s;
}