#include "Mapper_005.h"
#include <QString>
#include <QChar>

Mapper_005::Mapper_005(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_005::~Mapper_005()
{}

void Mapper_005::reset()
{
    prgMode = 3;
    chrMode = 3;
    extendedChrBank = 0;
    prgRamProtect1 = 0;
    prgRamProtect2 = 0;
    exRamMode = 0;
    ntMapping = 0x44; 
    fillTile = 0;
    fillAttr = 0;

    uint32_t prg8Count = GetPrg8Count();

    prgRamBank = 0;

    prgReg[0] = 0;
    prgReg[1] = 1;
    prgReg[2] = (prg8Count >= 2) ? uint8_t(prg8Count - 2) : 0;
    prgReg[3] = (prg8Count >= 1) ? uint8_t(prg8Count - 1) : 0;

    for (int i = 0; i < 8; i++)
        chrSpriteReg[i] = uint8_t(i);

    for (int i = 0; i < 4; i++)
        chrBgReg[i] = uint8_t(i);

    chrUpperBits = 0;

    for (int i = 0; i < 1024; i++)
        exRam[i] = 0;

    mulA = 0;
    mulB = 0;
    mulResult = 0;

    irqScanline = 0;
    irqStatus = 0;
    irqEnable = false;
    irqPending = false;
    for (int i = 0; i < 2; i++)
    {
        mmc5Pulse[i].control = 0;
        mmc5Pulse[i].timer = 0;
        mmc5Pulse[i].counter = 0;
        mmc5Pulse[i].sequence = 0;
        mmc5Pulse[i].enabled = false;
        mmc5Pulse[i].sample = 0.0f;
    }

    mmc5Status = 0;
    mmc5PcmDac = 0;
    mmc5PcmSample = 0.0f;
}

uint32_t Mapper_005::GetPrg8Count() const
{
    uint32_t count = nPRGBanks * 2;
    return count == 0 ? 1 : count;
}

uint32_t Mapper_005::GetChr1kCount() const
{
    uint32_t count = (nCHRBanks == 0) ? 8 : nCHRBanks * 8;
    return count == 0 ? 1 : count;
}

uint8_t Mapper_005::GetPrgBankReg(int index) const
{
    return prgReg[index & 3] & 0x7F;
}

uint8_t Mapper_005::GetNtSource(int ntIndex) const
{
    ntIndex &= 0x03;
    return (ntMapping >> (ntIndex * 2)) & 0x03;
}

uint8_t Mapper_005::ReadExRam(uint16_t offset) const
{
    return exRam[offset & 0x03FF];
}

void Mapper_005::WriteExRam(uint16_t offset, uint8_t data)
{
    exRam[offset & 0x03FF] = data;
}

uint8_t Mapper_005::GetFillTile() const
{
    return fillTile;
}

uint8_t Mapper_005::GetFillAttr() const
{
    // fillAttr chỉ có 2 bit, cần nhân ra 4 góc trong attribute byte
    return (fillAttr & 0x03) * 0x55;
}

uint8_t Mapper_005::GetExRamMode() const
{
    return exRamMode;
}

uint8_t Mapper_005::GetPrgRamBank() const
{
    return prgRamBank & 0x07;
}

void Mapper_005::SetExtendedChrBank(uint8_t bank)
{
    extendedChrBank = bank & 0x3F;
}

uint8_t Mapper_005::GetExtendedChrBank() const
{
    return extendedChrBank;
}
void Mapper_005::SetChrFetchModeBackground()
{
    chrFetchMode = ChrFetchMode::Background;
}

void Mapper_005::SetChrFetchModeSprite()
{
    chrFetchMode = ChrFetchMode::Sprite;
}
uint32_t Mapper_005::MapPrgBank8(uint8_t bank, uint16_t addr) const
{
    uint32_t prg8Count = GetPrg8Count();
    return (uint32_t(bank % prg8Count) * 0x2000) + (addr & 0x1FFF);
}

uint32_t Mapper_005::MapChrBank1K(uint32_t bank, uint16_t addr) const
{
    uint32_t chr1kCount = GetChr1kCount();
    return ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
}

bool Mapper_005::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    uint32_t prg8Count = GetPrg8Count();

    // PRG RAM area. Tùy Cartridge xử lý RAM ra sao,
    // map 8KB để không crash.
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return false;
    }

    if (addr < 0x8000)
        return false;

    // MMC5 PRG modes:
    // mode 0: 32KB
    // mode 1: 16KB + 16KB
    // mode 2: 16KB + 8KB + 8KB
    // mode 3: 8KB + 8KB + 8KB + 8KB

    switch (prgMode & 0x03)
    {
    case 0:
    {
        // 32KB at $8000-$FFFF, using $5117
        uint8_t bank = GetPrgBankReg(3) & 0x7C;
        mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x7FFF);
        return true;
    }

    case 1:
    {
        // $8000-$BFFF 16KB from $5115
        // $C000-$FFFF 16KB from $5117
        if (addr < 0xC000)
        {
            uint8_t bank = GetPrgBankReg(1) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        else
        {
            uint8_t bank = GetPrgBankReg(3) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        return true;
    }

    case 2:
    {
        // $8000-$BFFF 16KB from $5115
        // $C000-$DFFF 8KB from $5116
        // $E000-$FFFF 8KB from $5117
        if (addr < 0xC000)
        {
            uint8_t bank = GetPrgBankReg(1) & 0x7E;
            mapped_addr = ((bank % prg8Count) * 0x2000) + (addr & 0x3FFF);
        }
        else if (addr < 0xE000)
        {
            mapped_addr = MapPrgBank8(GetPrgBankReg(2), addr);
        }
        else
        {
            mapped_addr = MapPrgBank8(GetPrgBankReg(3), addr);
        }
        return true;
    }

    default:
    case 3:
    {
        // 4 x 8KB
        if (addr < 0xA000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(0), addr);
        else if (addr < 0xC000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(1), addr);
        else if (addr < 0xE000)
            mapped_addr = MapPrgBank8(GetPrgBankReg(2), addr);
        else
            mapped_addr = MapPrgBank8(GetPrgBankReg(3), addr);

        return true;
    }
    }
}

bool Mapper_005::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    // PRG RAM area
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        return false;
    }

    // MMC5 registers
    if (addr >= 0x5000 && addr <= 0x5FFF)
    {
        switch (addr)
        {
            // MMC5 expansion audio
        // Pulse 1: $5000, $5002, $5003
        case 0x5000:
        case 0x5002:
        case 0x5003:
            WriteMMC5PulseRegister(0, addr, data);
            return false;

            // Pulse 2: $5004, $5006, $5007
        case 0x5004:
        case 0x5006:
        case 0x5007:
            WriteMMC5PulseRegister(1, addr, data);
            return false;

            // PCM / DAC output
        case 0x5011:
            mmc5PcmDac = data & 0x7F;
            mmc5PcmSample = (float(mmc5PcmDac) - 64.0f) / 64.0f;
            return false;

            // MMC5 audio enable
        case 0x5015:
            mmc5Status = data;

            mmc5Pulse[0].enabled = (data & 0x01) != 0;
            mmc5Pulse[1].enabled = (data & 0x02) != 0;

            if (!mmc5Pulse[0].enabled)
                mmc5Pulse[0].sample = 0.0f;

            if (!mmc5Pulse[1].enabled)
                mmc5Pulse[1].sample = 0.0f;

            return false;

        case 0x5100:
            prgMode = data & 0x03;
            return false;

        case 0x5101:
            chrMode = data & 0x03;
            return false;

        case 0x5102:
            prgRamProtect1 = data;
            return false;

        case 0x5103:
            prgRamProtect2 = data;
            return false;

        case 0x5104:
            exRamMode = data & 0x03;
            return false;

        case 0x5105:
            ntMapping = data;
            return false;

        case 0x5106:
            fillTile = data;
            return false;

        case 0x5107:
            fillAttr = data & 0x03;
            return false;

        case 0x5113:
            prgRamBank = data & 0x07;
            return false;

        case 0x5114:
        case 0x5115:
        case 0x5116:
        case 0x5117:
            prgReg[addr - 0x5114] = data;
            return false;

        case 0x5120:
        case 0x5121:
        case 0x5122:
        case 0x5123:
        case 0x5124:
        case 0x5125:
        case 0x5126:
        case 0x5127:
            chrSpriteReg[addr - 0x5120] = data;
            return false;

        case 0x5128:
        case 0x5129:
        case 0x512A:
        case 0x512B:
            chrBgReg[addr - 0x5128] = data;
            return false;

        case 0x5130:
            chrUpperBits = data & 0x03;
            return false;

        case 0x5203:
            irqScanline = data;
            return false;

        case 0x5204:
            irqEnable = (data & 0x80) != 0;
            irqStatus &= ~0x80;
            irqPending = false;
            return false;

        case 0x5205:
            mulA = data;
            mulResult = uint16_t(mulA) * uint16_t(mulB);
            return false;

        case 0x5206:
            mulB = data;
            mulResult = uint16_t(mulA) * uint16_t(mulB);
            return false;

        default:
            break;
        }

        // ExRAM
        if (addr >= 0x5C00 && addr <= 0x5FFF)
        {
            exRam[addr & 0x03FF] = data;
            return false;
        }

        return false;
    }

    return false;
}

bool Mapper_005::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        uint32_t upper = uint32_t(chrUpperBits) << 8;
        uint8_t slot = (addr >> 10) & 0x07;      // sprite dùng 0-7
        uint8_t bgSlot = (addr >> 10) & 0x03;    // background dùng 0-3
        bool bgFetch = (chrFetchMode == ChrFetchMode::Background);
        // MMC5 ExRAM mode 1: Extended Attribute
        // Mỗi tile BG có thể chọn CHR 4KB page riêng bằng ExRAM.
        // bit 0-5 của ExRAM byte = CHR 4KB page
        // bit 6-7 của ExRAM byte = palette
        if (bgFetch && exRamMode == 1)
        {
            uint32_t bank = (uint32_t(extendedChrBank & 0x3F) << 2) + bgSlot;
            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }
        switch (chrMode & 0x03)
        {
        case 0:
        {
            // 8KB mode
            uint32_t bank;

            if (bgFetch)
            {
                // BG chỉ cần 4KB, dùng $512B làm base 4KB
                uint32_t base = upper | (chrBgReg[3] & 0xFC);
                bank = base + bgSlot;
            }
            else
            {
                uint32_t base = upper | (chrSpriteReg[7] & 0xF8);
                bank = base + slot;
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        case 1:
        {
            // 4KB mode
            uint32_t bank;

            if (bgFetch)
            {
                uint32_t base = upper | (chrBgReg[3] & 0xFC);
                bank = base + bgSlot;
            }
            else
            {
                uint32_t base = (slot < 4)
                    ? (upper | (chrSpriteReg[3] & 0xFC))
                    : (upper | (chrSpriteReg[7] & 0xFC));

                bank = base + (slot & 0x03);
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        case 2:
        {
            // 2KB mode
            uint32_t bank;

            if (bgFetch)
            {
                int pair = bgSlot >> 1;
                uint32_t base = upper | (chrBgReg[pair * 2 + 1] & 0xFE);
                bank = base + (bgSlot & 0x01);
            }
            else
            {
                int pair = slot >> 1;
                int regIndex = pair * 2 + 1;
                uint32_t base = upper | (chrSpriteReg[regIndex] & 0xFE);
                bank = base + (slot & 0x01);
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }

        default:
        case 3:
        {
            // 1KB mode
            uint32_t bank;

            if (bgFetch)
            {
                // Background dùng 4 reg: $5128-$512B
                bank = upper | chrBgReg[bgSlot];
            }
            else
            {
                // Sprite dùng 8 reg: $5120-$5127
                bank = upper | chrSpriteReg[slot];
            }

            mapped_addr = MapChrBank1K(bank, addr);
            return true;
        }
        }
    }

    // Nametable / ExRAM mapping của MMC5 phức tạp
    return false;
}

bool Mapper_005::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        return false;
    }

    return false;
}

bool Mapper_005::cpuReadRegister(uint16_t addr, uint8_t& data)
{
    if (addr >= 0x5C00 && addr <= 0x5FFF)
    {
        data = exRam[addr & 0x03FF];
        return true;
    }

    switch (addr)
    {
    case 0x5015:
    {
        data = 0x00;

        if (mmc5Pulse[0].enabled)
            data |= 0x01;

        if (mmc5Pulse[1].enabled)
            data |= 0x02;

        return true;
    }

    case 0x5204:
        data = irqStatus;
        irqStatus &= ~0x80;
        irqPending = false;
        return true;

    case 0x5205:
        data = mulResult & 0xFF;
        return true;

    case 0x5206:
        data = (mulResult >> 8) & 0xFF;
        return true;
    }

    return false;
}

void Mapper_005::NotifyScanline(int scanline)
{
    // Bit 6: PPU đang trong vùng render frame
    if (scanline >= 0 && scanline < 240)
        irqStatus |= 0x40;
    else
        irqStatus &= ~0x40;

    // Nếu IRQ enable và scanline khớp thanh ghi $5203
    if (irqEnable && scanline == irqScanline)
    {
        irqStatus |= 0x80;
        irqPending = true;
    }
}

bool Mapper_005::irqState()
{
    return irqPending;
}

void Mapper_005::irqClear()
{
    irqPending = false;
    irqStatus &= ~0x80;
}
MIRROR Mapper_005::mirror()
{
    // MMC5 thật dùng $5105 map từng nametable.
    // PPU hiện tại chỉ hỗ trợ MIRROR enum đơn giản.
    // Tạm suy ra kiểu gần nhất.
    uint8_t nt0 = ntMapping & 0x03;
    uint8_t nt1 = (ntMapping >> 2) & 0x03;
    uint8_t nt2 = (ntMapping >> 4) & 0x03;
    uint8_t nt3 = (ntMapping >> 6) & 0x03;

    if (nt0 == 0 && nt1 == 1 && nt2 == 0 && nt3 == 1)
        return MIRROR::VERTICAL;

    if (nt0 == 0 && nt1 == 0 && nt2 == 1 && nt3 == 1)
        return MIRROR::HORIZONTAL;

    if (nt0 == 0 && nt1 == 0 && nt2 == 0 && nt3 == 0)
        return MIRROR::ONESCREEN_LO;

    if (nt0 == 1 && nt1 == 1 && nt2 == 1 && nt3 == 1)
        return MIRROR::ONESCREEN_HI;

    return MIRROR::HARDWARE;
}

void Mapper_005::WriteMMC5PulseRegister(int ch, uint16_t addr, uint8_t data)
{
    MMC5Pulse& p = mmc5Pulse[ch & 1];

    switch (addr)
    {
    case 0x5000:
    case 0x5004:
        p.control = data;
        break;

    case 0x5002:
    case 0x5006:
        p.timer = (p.timer & 0x0700) | data;
        break;
    case 0x5003:
    case 0x5007:
        p.timer = (p.timer & 0x00FF) | ((data & 0x07) << 8);
        p.sequence = 0;
        p.counter = (p.timer + 1) * 2;
        break;
    }
}

float Mapper_005::GetPulseOutput(const MMC5Pulse& p) const
{
    if (!p.enabled)
        return 0.0f;

    if (p.timer < 8)
        return 0.0f;

    uint8_t volume = p.control & 0x0F;

    if (volume == 0)
        return 0.0f;

    uint8_t duty = (p.control >> 6) & 0x03;

    static const uint8_t dutyTable[4][8] =
    {
        {0,1,0,0,0,0,0,0}, // 12.5%
        {0,1,1,0,0,0,0,0}, // 25%
        {0,1,1,1,1,0,0,0}, // 50%
        {1,0,0,1,1,1,1,1}  // 25% inverted
    };

    float amp = float(volume) / 15.0f;

    return dutyTable[duty][p.sequence & 7] ? amp : -amp;
}

void Mapper_005::ClockAudio()
{
    for (int i = 0; i < 2; i++)
    {
        MMC5Pulse& p = mmc5Pulse[i];

        if (!p.enabled || p.timer < 8)
        {
            p.sample = 0.0f;
            continue;
        }

        if (p.counter == 0)
        {
            p.counter = (p.timer + 1) * 2;
            p.sequence = (p.sequence + 1) & 0x07;
        }
        else
        {
            p.counter--;
        }

        p.sample = GetPulseOutput(p);
    }
}

float Mapper_005::GetMMC5Pulse1Sample() const
{
    return mmc5Pulse[0].sample;
}

float Mapper_005::GetMMC5Pulse2Sample() const
{
    return mmc5Pulse[1].sample;
}

float Mapper_005::GetMMC5PCMSample() const
{
    return mmc5PcmSample;
}

void Mapper_005::GetMMC5DebugChannels(float& pulse1, float& pulse2, float& pcm) const
{
    pulse1 = GetMMC5Pulse1Sample();
    pulse2 = GetMMC5Pulse2Sample();
    pcm = GetMMC5PCMSample();
}

void Mapper_005::GetMMC5DebugPeriods(float& pulse1, float& pulse2) const
{
    constexpr float CPU_TO_SAMPLE = 44100.0f / 1789773.0f;
    auto calc = [](const MMC5Pulse& p) -> float {
        constexpr float CPU_TO_SAMPLE_LOCAL = 44100.0f / 1789773.0f;
        if (!p.enabled || p.timer < 8)
            return 0.0f;

        // MMC5 pulse dùng duty 8 bước, clock theo CPU cycle trong SJNES.
        float samples = float(p.timer + 1) * 16.0f * CPU_TO_SAMPLE_LOCAL;
        if (samples < 2.0f) return 0.0f;
        if (samples > 8192.0f) return 8192.0f;
        return samples;
        };

    pulse1 = calc(mmc5Pulse[0]);
    pulse2 = calc(mmc5Pulse[1]);
}


void Mapper_005::GetMMC5DebugDuty(float& pulse1, float& pulse2) const
{
    auto calc = [](const MMC5Pulse& p) -> float {
        if (!p.enabled || p.timer < 8)
            return -1.0f;
        return float((p.control >> 6) & 0x03);
    };

    pulse1 = calc(mmc5Pulse[0]);
    pulse2 = calc(mmc5Pulse[1]);
}

float Mapper_005::GetExpansionAudio()
{
    float p1 = GetMMC5Pulse1Sample();
    float p2 = GetMMC5Pulse2Sample();
    float pcm = GetMMC5PCMSample();

    return (p1 * 0.12f) + (p2 * 0.12f) + (pcm * 0.10f);
}

std::string Mapper_005::GetDebugInfo()
{
    std::string s;

    auto mirrorToString = [](MIRROR m) -> std::string {
        switch (m)
        {
        case MIRROR::HORIZONTAL:   return "Horizontal / Ngang";
        case MIRROR::VERTICAL:     return "Vertical / Doc";
        case MIRROR::ONESCREEN_LO: return "One-screen thap";
        case MIRROR::ONESCREEN_HI: return "One-screen cao";
        case MIRROR::HARDWARE:     return "MMC5 nametable mapping phuc tap";
        default:                   return "Khong ro";
        }
        };

    uint32_t prg8Count = GetPrg8Count();
    uint32_t chr1kCount = GetChr1kCount();

    s += "===== MAPPER 005 - MMC5 / EXROM =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "MMC5 la mapper nang cao cua Nintendo, co PRG/CHR banking nhieu mode, ExRAM, IRQ, multiplier va audio mo rong.\n";
    s += "Ban hien tai la phase 1, chua phai MMC5 full chinh xac.\n\n";

    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(prg8Count) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(chr1kCount) + "\n";
    s += "Mirroring gan dung: " + mirrorToString(mirror()) + "\n";

    s += "\nTHANH GHI CONTROL:\n";
    s += "$5100 PRG Mode     : " + std::to_string(prgMode) + "\n";
    s += "$5101 CHR Mode     : " + std::to_string(chrMode) + "\n";
    s += "$5104 ExRAM Mode   : " + std::to_string(exRamMode) + "\n";
    s += "$5105 NT Mapping   : 0x" + HexStr(ntMapping, 2) + "\n";
    s += "$5106 Fill Tile    : " + std::to_string(fillTile) + "\n";
    s += "$5107 Fill Attr    : " + std::to_string(fillAttr) + "\n";

    s += "\nPRG REGISTERS:\n";
    for (int i = 0; i < 4; i++)
    {
        s += "$511" + std::to_string(i + 4) + " = 0x" + HexStr(prgReg[i], 2) +
            " | bank=" + std::to_string(prgReg[i] & 0x7F) +
            " | ROM/RAM bit=" + std::string((prgReg[i] & 0x80) ? "ROM" : "RAM") + "\n";
    }

    s += "\nCHR SPRITE REGISTERS $5120-$5127:\n";
    for (int i = 0; i < 8; i++)
    {
        s += "CHR sprite[" + std::to_string(i) + "] = " + std::to_string(chrSpriteReg[i]) + "\n";
    }

    s += "\nCHR BG REGISTERS $5128-$512B:\n";
    for (int i = 0; i < 4; i++)
    {
        s += "CHR bg[" + std::to_string(i) + "] = " + std::to_string(chrBgReg[i]) + "\n";
    }

    s += "CHR upper bits $5130: " + std::to_string(chrUpperBits) + "\n";

    s += "\nIRQ / MULTIPLIER:\n";
    s += "IRQ scanline : " + std::to_string(irqScanline) + "\n";
    s += std::string("IRQ enable   : ") + (irqEnable ? "BAT" : "TAT") + "\n";
    s += "IRQ status   : 0x" + HexStr(irqStatus, 2) + "\n";
    s += "Multiplier   : " + std::to_string(mulA) + " x " + std::to_string(mulB) + " = " + std::to_string(mulResult) + "\n";

    s += "\nGHI CHU:\n";
    s += "MMC5 full can sua them PPU de phan biet background/sprite CHR fetch, ExRAM nametable, fill mode, vertical split va IRQ scanline.\n";
    s += "Ban nay dung de bat dau boot/test game Mapper 5 truoc.\n";

    return s;
}