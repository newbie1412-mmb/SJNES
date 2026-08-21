#include "Mapper_023.h"

Mapper_023::Mapper_023(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_023::~Mapper_023() {}

void Mapper_023::reset() {
    mirrormode = MIRROR::VERTICAL;

    // Khởi tạo đồ họa sạch sẽ
    for (int i = 0; i < 8; i++) {
        nCHRBank_Lo[i] = 0;
        nCHRBank_Hi[i] = 0;
        pCHRBank[i] = 0;
    }

    // Khởi tạo Code: 2 Bank đầu tiên, và 2 Bank cuối bị KHÓA CỨNG 
    pPRGBank[0] = 0 * 0x2000;
    pPRGBank[1] = 1 * 0x2000;
    pPRGBank[2] = (nPRGBanks * 2 - 2) * 0x2000; // Bank áp chót
    pPRGBank[3] = (nPRGBanks * 2 - 1) * 0x2000; // Bank cuối cùng
}

bool Mapper_023::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) { mapped_addr = pPRGBank[0] + (addr & 0x1FFF); return true; }
    if (addr >= 0xA000 && addr <= 0xBFFF) { mapped_addr = pPRGBank[1] + (addr & 0x1FFF); return true; }
    if (addr >= 0xC000 && addr <= 0xDFFF) { mapped_addr = pPRGBank[2] + (addr & 0x1FFF); return true; }
    if (addr >= 0xE000 && addr <= 0xFFFF) { mapped_addr = pPRGBank[3] + (addr & 0x1FFF); return true; }
    return false;
}

bool Mapper_023::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        if (addr >= 0x8000 && addr <= 0x8FFF) {
            // Đổi PRG Bank 0
            pPRGBank[0] = (data & 0x1F) % (nPRGBanks * 2) * 0x2000;
        }
        else if (addr >= 0x9000 && addr <= 0x9FFF) {
            // Lật trang màn hình (Mirroring) trên không trung
            switch (data & 0x03) {
            case 0: mirrormode = MIRROR::VERTICAL; break;
            case 1: mirrormode = MIRROR::HORIZONTAL; break;
            case 2: mirrormode = MIRROR::ONESCREEN_LO; break;
            case 3: mirrormode = MIRROR::ONESCREEN_HI; break;
            }
        }
        else if (addr >= 0xA000 && addr <= 0xAFFF) {
            // Đổi PRG Bank 1
            pPRGBank[1] = (data & 0x1F) % (nPRGBanks * 2) * 0x2000;
        }
        else if (addr >= 0xB000 && addr <= 0xEFFF) {
            //  GHÉP MẢNH 4-BIT CỦA 

            // Tính xem game đang muốn đổi hình ảnh cho ô số mấy (0 đến 7)
            int bank_index = ((addr >> 12) - 0xB) * 2 + ((addr >> 1) & 0x01);

            // Phân loại xem mảnh vỡ này là nửa cao hay nửa thấp
            bool is_high = (addr & 0x01);

            if (is_high) {
                nCHRBank_Hi[bank_index] = data & 0x0F;
            }
            else {
                nCHRBank_Lo[bank_index] = data & 0x0F;
            }

            // Dùng keo 502 dán 2 nửa lại với nhau thành 1 số 8-bit hoàn chỉnh
            uint8_t full_bank = (nCHRBank_Hi[bank_index] << 4) | nCHRBank_Lo[bank_index];

            // Map địa chỉ ra thực tế
            uint32_t max_chr_banks = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
            pCHRBank[bank_index] = (full_bank % max_chr_banks) * 0x0400;
        }

        // LUÔN TRẢ VỀ FALSE ĐỂ TRÁNH BUG GHI ĐÈ ROM GIỐNG TRẬN BATTLETOADS
        return false;
    }
    return false;
}

bool Mapper_023::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Ánh xạ 8 ô cửa sổ 1KB cực kỳ nhanh gọn
        uint8_t bank_index = addr / 0x0400;
        mapped_addr = pCHRBank[bank_index] + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper_023::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (nCHRBanks == 0) { // Nếu game xài CHR-RAM
            uint8_t bank_index = addr / 0x0400;
            mapped_addr = pCHRBank[bank_index] + (addr & 0x03FF);
            return true;
        }
    }
    return false;
}

MIRROR Mapper_023::mirror() {
    return mirrormode;
}
std::string Mapper_023::GetDebugInfo()
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

    s += "===== MAPPER 023 - VRC4 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(nPRGBanks * 2) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(nCHRBanks == 0 ? 8 : nCHRBanks * 8) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrormode) + "\n";

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
        uint8_t fullBank = (nCHRBank_Hi[i] << 4) | nCHRBank_Lo[i];
        uint16_t start = i * 0x0400;

        s += "$" + HexStr(start, 4) + "-$" + HexStr(start + 0x03FF, 4) +
            " : Lo=" + std::to_string(nCHRBank_Lo[i]) +
            " Hi=" + std::to_string(nCHRBank_Hi[i]) +
            " Full=" + std::to_string(fullBank) +
            " | offset CHR=0x" + HexStr(pCHRBank[i], 6) +
            " | CHR bank 1KB=" + std::to_string(pCHRBank[i] / 0x0400) + "\n";
    }

    s += "\nGHI CHU:\n";
    s += "Mapper 023 la VRC4. CHR bank duoc ghep tu 2 manh 4-bit Lo/Hi.\n";
    s += "PRG bank $C000-$FFFF thuong co dinh o hai bank cuoi.\n";

    return s;
}