#include "Mapper_025.h"

Mapper_025::Mapper_025(uint8_t prgBanks, uint8_t chrBanks) : Mapper_021(prgBanks, chrBanks) {}

Mapper_025::~Mapper_025() {}

bool Mapper_025::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr >= 0x8000) {
        // VRC4c (Mapper 025): Dùng A9, A8, A7 và A1, A0 để decode register
        // Công thức: bits A9-A7 shift xuống 4 bits, kết hợp với A1-A0
        // Sau đó swap sang Mapper_021 format (A11, A10) rồi gọi parent

        uint16_t reg25 = ((addr >> 7) & 0x70) | (addr & 0x03);

        // Swap từ Mapper 25 register sang Mapper 21 register
        uint16_t reg21;
        switch (reg25) {
        case 0x00: reg21 = 0x8000; break;  // $8xxx -> $8000
        case 0x01: reg21 = 0x8001; break;  // $8xxx -> $8001
        case 0x02: reg21 = 0x8002; break;  // $8xxx -> $8002
        case 0x03: reg21 = 0x8003; break;  // $8xxx -> $8003
        case 0x10: reg21 = 0x9000; break;  // $9xxx -> $9000
        case 0x11: reg21 = 0x9001; break;
        case 0x12: reg21 = 0x9002; break;
        case 0x13: reg21 = 0x9003; break;  // $9xxx -> $9003
        case 0x20: reg21 = 0xA000; break;  // $Axxx -> $A000
        case 0x21: reg21 = 0xA001; break;
        case 0x22: reg21 = 0xA002; break;
        case 0x23: reg21 = 0xA003; break;
        case 0x30: reg21 = 0xB000; break;  // $Bxxx CHR banks
        case 0x31: reg21 = 0xB001; break;
        case 0x32: reg21 = 0xB002; break;
        case 0x33: reg21 = 0xB003; break;
        case 0x40: reg21 = 0xC000; break;  // $Cxxx CHR banks
        case 0x41: reg21 = 0xC001; break;
        case 0x42: reg21 = 0xC002; break;
        case 0x43: reg21 = 0xC003; break;
        case 0x50: reg21 = 0xD000; break;  // $Dxxx CHR banks
        case 0x51: reg21 = 0xD001; break;
        case 0x52: reg21 = 0xD002; break;
        case 0x53: reg21 = 0xD003; break;
        case 0x60: reg21 = 0xE000; break;  // $Exxx CHR banks
        case 0x61: reg21 = 0xE001; break;
        case 0x62: reg21 = 0xE002; break;
        case 0x63: reg21 = 0xE003; break;
        case 0x70: reg21 = 0xF000; break;  // $Fxxx IRQ
        case 0x71: reg21 = 0xF001; break;
        case 0x72: reg21 = 0xF002; break;
        case 0x73: reg21 = 0xF003; break;
        default:   return false;
        }

        // Gọi parent class (Mapper_021) với register đã swap
        return Mapper_021::cpuMapWrite(reg21, mapped_addr, data);
    }
    return false;
}
std::string Mapper_025::GetDebugInfo()
{
    // Lấy debug info từ parent (Mapper_021), chỉ thay đổi title
    std::string s = Mapper_021::GetDebugInfo();

    // Thay "MAPPER 021" thành "MAPPER 025"
    size_t pos = s.find("MAPPER 021");
    if (pos != std::string::npos) {
        s.replace(pos, 10, "MAPPER 025");
    }

    // Thêm note về wiring khác
    size_t notePos = s.find("GHI CHU:");
    if (notePos != std::string::npos) {
        size_t endPos = s.find("\n", notePos);
        if (endPos != std::string::npos) {
            s.insert(endPos, "\nKhac Mapper 021 o wiring bit address (dung A9-A7 thay vi A11-A10).");
        }
    }

    return s;
}