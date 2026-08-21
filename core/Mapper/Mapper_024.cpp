#include "Mapper_024.h"
#include <algorithm>
Mapper_024::Mapper_024(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_024::~Mapper_024() {}

void Mapper_024::reset()
{
    prg16Bank = 0;
    prg8Bank = 0;

    for (int i = 0; i < 8; i++)
        chrBank[i] = i;

    UpdateB003(0x20);

    irqActive = false;
    irqEnable = false;
    irqEnableAfterAck = false;
    irqModeCycle = false;
    irqLatch = 0;
    irqCounter = 0;
    irqPrescaler = 341;

    pulse1 = VRC6Pulse{};
    pulse2 = VRC6Pulse{};
    saw = VRC6Saw{};
}

void Mapper_024::UpdateB003(uint8_t data)
{
    b003 = data;
    ppuBankingMode = data & 0x03;
    useChrRomNametables = (data & 0x10) != 0;
    chrA10Control = (data & 0x20) != 0;
    prgRamEnable = (data & 0x80) != 0;

    auto SetNametable = [&](int index, uint8_t page)
        {
            ntSource[index & 0x03] = page & 0x01;
        };

    auto SetChrNametable = [&](int index, uint8_t page)
        {
            ntChrBank[index & 0x03] = page;
        };

    if (useChrRomNametables)
    {
        // Same nametable/CHR-ROM banking rules as Mesen's VRC6::UpdatePpuBanking().
        switch (b003 & 0x2F)
        {
        case 0x20:
        case 0x27:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(2, chrBank[7] & 0xFE);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        case 0x23:
        case 0x24:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, chrBank[7] & 0xFE);
            SetChrNametable(2, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        case 0x28:
        case 0x2F:
            SetChrNametable(0, chrBank[6] & 0xFE);
            SetChrNametable(1, chrBank[6] & 0xFE);
            SetChrNametable(2, chrBank[7] & 0xFE);
            SetChrNametable(3, chrBank[7] & 0xFE);
            break;

        case 0x2B:
        case 0x2C:
            SetChrNametable(0, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(1, (chrBank[7] & 0xFE) | 1);
            SetChrNametable(2, (chrBank[6] & 0xFE) | 1);
            SetChrNametable(3, (chrBank[7] & 0xFE) | 1);
            break;

        default:
            switch (b003 & 0x07)
            {
            case 0:
            case 6:
            case 7:
                SetChrNametable(0, chrBank[6]);
                SetChrNametable(1, chrBank[6]);
                SetChrNametable(2, chrBank[7]);
                SetChrNametable(3, chrBank[7]);
                break;

            case 1:
            case 5:
                SetChrNametable(0, chrBank[4]);
                SetChrNametable(1, chrBank[5]);
                SetChrNametable(2, chrBank[6]);
                SetChrNametable(3, chrBank[7]);
                break;

            case 2:
            case 3:
            case 4:
                SetChrNametable(0, chrBank[6]);
                SetChrNametable(1, chrBank[7]);
                SetChrNametable(2, chrBank[6]);
                SetChrNametable(3, chrBank[7]);
                break;
            }
            break;
        }
        return;
    }

    // CIRAM nametable banking. mirrorMode is still maintained for the PPU's
    // simple mirroring path, while ntSource[] keeps the per-nametable mapping.
    switch (b003 & 0x2F)
    {
    case 0x20:
    case 0x27:
        mirrorMode = MIRROR::VERTICAL;
        SetNametable(0, 0); SetNametable(1, 1); SetNametable(2, 0); SetNametable(3, 1);
        break;

    case 0x23:
    case 0x24:
        mirrorMode = MIRROR::HORIZONTAL;
        SetNametable(0, 0); SetNametable(1, 0); SetNametable(2, 1); SetNametable(3, 1);
        break;

    case 0x28:
    case 0x2F:
        mirrorMode = MIRROR::ONESCREEN_LO;
        SetNametable(0, 0); SetNametable(1, 0); SetNametable(2, 0); SetNametable(3, 0);
        break;

    case 0x2B:
    case 0x2C:
        mirrorMode = MIRROR::ONESCREEN_HI;
        SetNametable(0, 1); SetNametable(1, 1); SetNametable(2, 1); SetNametable(3, 1);
        break;

    default:
        switch (b003 & 0x07)
        {
        case 0:
        case 6:
        case 7:
            SetNametable(0, chrBank[6]);
            SetNametable(1, chrBank[6]);
            SetNametable(2, chrBank[7]);
            SetNametable(3, chrBank[7]);
            // This can be more complex than a simple MIRROR enum, but keeping the
            // closest common value helps older PPU code paths.
            mirrorMode = ((chrBank[6] & 1) == 0 && (chrBank[7] & 1) == 1) ? MIRROR::VERTICAL :
                ((chrBank[6] & 1) == 0 ? MIRROR::ONESCREEN_LO : MIRROR::ONESCREEN_HI);
            break;

        case 1:
        case 5:
            SetNametable(0, chrBank[4]);
            SetNametable(1, chrBank[5]);
            SetNametable(2, chrBank[6]);
            SetNametable(3, chrBank[7]);
            mirrorMode = MIRROR::VERTICAL;
            break;

        case 2:
        case 3:
        case 4:
            SetNametable(0, chrBank[6]);
            SetNametable(1, chrBank[7]);
            SetNametable(2, chrBank[6]);
            SetNametable(3, chrBank[7]);
            mirrorMode = MIRROR::VERTICAL;
            break;
        }
        break;
    }
}

uint8_t Mapper_024::GetChrRegisterForPpuAddress(uint16_t addr) const
{
    const uint8_t slot = (addr >> 10) & 0x07;

    switch (ppuBankingMode & 0x03)
    {
    default:
    case 0:
        // 8 x 1KB: R0 R1 R2 R3 R4 R5 R6 R7
        return slot;

    case 1:
        // 4 x 2KB: R0 R0 R1 R1 R2 R2 R3 R3
        return slot >> 1;

    case 2:
    case 3:
        // 4 x 1KB + 2 x 2KB:
        // R0 R1 R2 R3 R4 R4 R5 R5
        if (slot < 4)
            return slot;
        return 4 + ((slot - 4) >> 1);
    }
}

uint8_t Mapper_024::GetChrBankForPpuAddress(uint16_t addr) const
{
    const uint8_t slot = (addr >> 10) & 0x07;
    const uint8_t reg = GetChrRegisterForPpuAddress(addr) & 0x07;
    uint8_t bank = chrBank[reg];

    const bool uses2K =
        ((ppuBankingMode & 0x03) == 1) ||
        (((ppuBankingMode & 0x03) == 2 || (ppuBankingMode & 0x03) == 3) && slot >= 4);

    if (uses2K && chrA10Control)
        bank = (bank & 0xFE) | (slot & 0x01);

    return bank;
}

bool Mapper_024::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    const uint32_t prg8Count = nPRGBanks * 2;
    if (prg8Count == 0)
        return false;

    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        const uint32_t bank16Count = nPRGBanks;
        mapped_addr = ((prg16Bank % bank16Count) * 0x4000) + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        mapped_addr = ((prg8Bank % prg8Count) * 0x2000) + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xE000 && addr <= 0xFFFF)
    {
        mapped_addr = ((prg8Count - 1) * 0x2000) + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_024::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;
    const uint16_t reg = addr & 0xF003;

    switch (reg)
    {
    case 0x8000:
    case 0x8001:
    case 0x8002:
    case 0x8003:
        prg16Bank = data & 0x0F;
        return false;

    case 0x9000:
    case 0x9001:
    case 0x9002:
        WritePulse(pulse1, reg, data);
        return false;

    case 0x9003:
        return false;

    case 0xA000:
    case 0xA001:
    case 0xA002:
        WritePulse(pulse2, reg, data);
        return false;

    case 0xA003:
        return false;

    case 0xB000:
    case 0xB001:
    case 0xB002:
        WriteSaw(reg, data);
        return false;

    case 0xB003:
        UpdateB003(data);
        return false;

    case 0xC000:
    case 0xC001:
    case 0xC002:
    case 0xC003:
        prg8Bank = data & 0x1F;
        return false;

    case 0xD000: chrBank[0] = data; UpdateB003(b003); return false;
    case 0xD001: chrBank[1] = data; UpdateB003(b003); return false;
    case 0xD002: chrBank[2] = data; UpdateB003(b003); return false;
    case 0xD003: chrBank[3] = data; UpdateB003(b003); return false;

    case 0xE000: chrBank[4] = data; UpdateB003(b003); return false;
    case 0xE001: chrBank[5] = data; UpdateB003(b003); return false;
    case 0xE002: chrBank[6] = data; UpdateB003(b003); return false;
    case 0xE003: chrBank[7] = data; UpdateB003(b003); return false;

    case 0xF000:
        irqLatch = data;
        irqActive = false;
        return false;

    case 0xF001:
        irqEnableAfterAck = (data & 0x01) != 0;
        irqEnable = (data & 0x02) != 0;
        irqModeCycle = (data & 0x04) != 0;

        irqActive = false;
        irqPrescaler = 341;

        if (irqEnable)
            irqCounter = irqLatch;

        return false;

    case 0xF002:
        irqActive = false;
        irqEnable = irqEnableAfterAck;
        return false;
    }

    return false;
}

bool Mapper_024::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        const uint8_t bank = GetChrBankForPpuAddress(addr);
        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (chr1kCount == 0)
            return false;

        mapped_addr = ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
        return true;
    }

    // VRC6 can map nametable space ($2000-$2FFF, mirrored at $3000-$3EFF)
    // to CHR ROM/RAM when $B003 bit 4 is set. This is required by games that
    // use CHR-ROM nametables for split backgrounds.
    if (addr >= 0x2000 && addr <= 0x3EFF && useChrRomNametables)
    {
        const uint16_t ntAddr = (addr - 0x2000) & 0x0FFF;
        const uint8_t ntIndex = (ntAddr >> 10) & 0x03;
        const uint16_t offset = ntAddr & 0x03FF;

        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
        if (chr1kCount == 0)
            return false;

        mapped_addr = ((ntChrBank[ntIndex] % chr1kCount) * 0x0400) + offset;
        return true;
    }

    return false;
}

bool Mapper_024::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        const uint8_t bank = GetChrBankForPpuAddress(addr);
        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (chr1kCount == 0)
            return false;

        mapped_addr = ((bank % chr1kCount) * 0x0400) + (addr & 0x03FF);
        return nCHRBanks == 0;
    }

    if (addr >= 0x2000 && addr <= 0x3EFF && useChrRomNametables)
    {
        const uint16_t ntAddr = (addr - 0x2000) & 0x0FFF;
        const uint8_t ntIndex = (ntAddr >> 10) & 0x03;
        const uint16_t offset = ntAddr & 0x03FF;

        const uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);
        if (chr1kCount == 0)
            return false;

        mapped_addr = ((ntChrBank[ntIndex] % chr1kCount) * 0x0400) + offset;
        return nCHRBanks == 0;
    }

    return false;
}

void Mapper_024::irqStep()
{
    ClockVRC6Audio();

    if (!irqEnable)
        return;

    auto clockIRQCounter = [&]()
        {
            if (irqCounter == 0xFF)
            {
                irqCounter = irqLatch;
                irqActive = true;
            }
            else
            {
                irqCounter++;
            }
        };

    if (irqModeCycle)
    {
        clockIRQCounter();
    }
    else
    {
        irqPrescaler -= 3;

        if (irqPrescaler <= 0)
        {
            irqPrescaler += 341;
            clockIRQCounter();
        }
    }
}

bool Mapper_024::irqState()
{
    return irqActive;
}

void Mapper_024::irqClear()
{
    irqActive = false;
}

MIRROR Mapper_024::mirror()
{
    return mirrorMode;
}

void Mapper_024::WritePulse(VRC6Pulse& p, uint16_t reg, uint8_t data)
{
    switch (reg & 0x0003)
    {
    case 0:
        p.volume = data & 0x0F;
        p.duty = (data >> 4) & 0x07;
        p.mode = (data & 0x80) != 0;
        break;

    case 1:
        p.period = (p.period & 0x0F00) | data;
        break;

    case 2:
        p.period = (p.period & 0x00FF) | ((data & 0x0F) << 8);
        p.enable = (data & 0x80) != 0;

        if (!p.enable)
        {
            p.output = 0.0f;
            p.dutyPos = 0;
        }
        break;
    }
}

void Mapper_024::WriteSaw(uint16_t reg, uint8_t data)
{
    switch (reg & 0x0003)
    {
    case 0:
        saw.rate = data & 0x3F;
        break;

    case 1:
        saw.period = (saw.period & 0x0F00) | data;
        break;

    case 2:
        saw.period = (saw.period & 0x00FF) | ((data & 0x0F) << 8);
        saw.enable = (data & 0x80) != 0;

        if (!saw.enable)
        {
            saw.output = 0.0f;
            saw.prev_output = 0.0f;
            saw.step = 0;
            saw.accumulator = 0;
            saw.phase = 0.0f;
        }
        break;
    }
}

void Mapper_024::ClockVRC6Audio()
{
    auto clockPulse = [](VRC6Pulse& p)
        {
            if (!p.enable || p.period < 8)
            {
                p.output = 0.0f;
                return;
            }

            if (p.timer == 0)
            {
                p.timer = p.period;
                p.dutyPos = (p.dutyPos + 1) & 0x0F;
            }
            else
            {
                p.timer--;
            }

            if (p.mode)
            {
                p.output = static_cast<float>(p.volume) / 15.0f;
            }
            else
            {
                const bool high = p.dutyPos <= p.duty;
                p.output = high ? (static_cast<float>(p.volume) / 15.0f) : 0.0f;
            }
        };


    clockPulse(pulse1);
    clockPulse(pulse2);

    if (!saw.enable || saw.period < 8)
    {
        saw.output = 0.0f;
        saw.prev_output = 0.0f;
        saw.filtered_output = 0.0f;
        saw.phase = 0.0f;
        saw.timer = saw.period;
        saw.debugTimer = saw.period;
        saw.debugSubstepPeriod = saw.period;
        saw.debugStep = 0;
        saw.debugCyclePeak = 0.0f;
        return;
    }

    // Snap debugCyclePeak ngay khi note VỪA bật lại (step==0, timer chưa tick lần nào).
    if (saw.debugStep == 0 && saw.debugTimer == saw.period)
    {
        saw.debugCyclePeak = std::clamp(static_cast<float>(saw.rate) * 7.0f / 8.0f, 0.0f, 31.0f);
    }

    // Bộ đếm riêng cho oscilloscope, LUÔN tick song song bất kể smoothSawEnabled,
    // không đụng gì tới audio thật (saw.output/saw.timer/saw.step ở dưới).
    if (saw.debugTimer == 0)
    {
        saw.debugTimer = saw.period;
        saw.debugSubstepPeriod = saw.period; 
        if (saw.debugStep == 13) // sắp wrap về 0 ở bước tăng ngay dưới
        {
            saw.debugCyclePeak = std::clamp(static_cast<float>(saw.rate) * 7.0f / 8.0f, 0.0f, 31.0f);
        }

        saw.debugStep++;
        if (saw.debugStep >= 14)
            saw.debugStep = 0;
    }
    else
    {
        saw.debugTimer--;
    }

    if (smoothSawEnabled)
    {
        const float cycleLength = static_cast<float>(saw.period + 1) * 14.0f;
        saw.phase += 1.0f / cycleLength;

        if (saw.phase >= 1.0f)
            saw.phase -= 1.0f;

        float peak = static_cast<float>(saw.rate) * 7.0f / 8.0f;
        if (peak > 31.0f)
            peak = 31.0f;

        saw.prev_output = saw.output;
        saw.output = (saw.phase * peak) / 31.0f;
    }
    else
    {
        if (saw.timer == 0)
        {
            saw.timer = saw.period;

            if (saw.step == 0)
                saw.accumulator = 0;

            if (saw.step & 1)
                saw.accumulator += saw.rate;

            saw.step++;

            if (saw.step >= 14)
            {
                saw.step = 0;
                saw.accumulator = 0;
            }

            saw.prev_output = saw.output;
            saw.output = static_cast<float>((saw.accumulator >> 3) & 0x1F) / 31.0f;
        }
        else
        {
            saw.timer--;
        }
    }
}

float Mapper_024::GetExpansionAudio()
{
    if (muteVRC6) return 0.0f;

    float p1 = pulse1.output * 20.0f;
    float p2 = pulse2.output * 20.0f;
    constexpr float sawGain = 1.8f;
    float s = saw.GetCleanOutput(smoothSawEnabled) * 31.0f * sawGain;
    float vrc6_mix = (p1 + p2 + s) / (40.0f + 31.0f * sawGain);
    return vrc6_mix;
}

void Mapper_024::GetExpansionAudioStereo(float& left, float& right)
{
    if (muteVRC6)
    {
        left = 0.0f;
        right = 0.0f;
        return;
    }

    float p1 = pulse1.output * 20.0f;
    float p2 = pulse2.output * 20.0f;
    constexpr float sawGain = 1.8f;
    float s = saw.GetCleanOutput(smoothSawEnabled) * 31.0f * sawGain;

    float mixL = (p1 * 0.60f) + (p2 * 0.40f) + (s * 0.50f);
    float mixR = (p1 * 0.40f) + (p2 * 0.60f) + (s * 0.50f);

    const float stereoNorm = 20.0f + 20.0f * sawGain;
    left = mixL / stereoNorm;
    right = mixR / stereoNorm;
}
void Mapper_024::GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3)
{
    if (muteVRC6)
    {
        ch1 = 0.0f;
        ch2 = 0.0f;
        ch3 = 0.0f;
        return;
    }
    ch1 = pulse1.output;
    ch2 = pulse2.output;
    if (smoothSawEnabled && saw.enable && saw.period >= 8)
    {
        float frac = static_cast<float>(saw.debugSubstepPeriod - saw.debugTimer) / static_cast<float>(saw.debugSubstepPeriod + 1);
        frac = std::clamp(frac, 0.0f, 1.0f);
        float rampProgress = (static_cast<float>(saw.debugStep) + frac) / 14.0f;
        ch3 = std::clamp(rampProgress * (saw.debugCyclePeak / 31.0f), 0.0f, 1.0f);
    }
    else
    {
        ch3 = saw.output;
    }
}

void Mapper_024::GetVRC6DebugPeriods(float& ch1, float& ch2, float& ch3) const
{
    constexpr float CPU_TO_SAMPLE = 44100.0f / 1789773.0f;
    auto clampPeriod = [](float v) -> float {
        if (v < 2.0f) return 0.0f;
        if (v > 8192.0f) return 8192.0f;
        return v;
        };

    ch1 = (!muteVRC6 && pulse1.enable && pulse1.period >= 8)
        ? clampPeriod(float(pulse1.period + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    ch2 = (!muteVRC6 && pulse2.enable && pulse2.period >= 8)
        ? clampPeriod(float(pulse2.period + 1) * 16.0f * CPU_TO_SAMPLE)
        : 0.0f;

    // VRC6 saw: 14 steps / chu kỳ.
    ch3 = (!muteVRC6 && saw.enable && saw.period >= 8)
        ? clampPeriod(float(saw.period + 1) * 14.0f * CPU_TO_SAMPLE)
        : 0.0f;
}


void Mapper_024::GetVRC6DebugDuty(float& ch1, float& ch2) const
{
    auto calc = [&](const VRC6Pulse& p) -> float {
        if (muteVRC6 || !p.enable || p.period < 8 || p.volume == 0)
            return -1.0f;

        if (p.mode)
            return 15.0f;

        return float(p.duty & 0x07);
        };

    ch1 = calc(pulse1);
    ch2 = calc(pulse2);
}

std::string Mapper_024::GetDebugInfo()
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

    uint32_t prg8Count = nPRGBanks * 2;
    uint32_t chr1kCount = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

    s += "===== MAPPER 024 - KONAMI VRC6 =====\n\n";

    s += "THONG TIN CHUNG:\n";
    s += "So PRG banks 16KB : " + std::to_string(nPRGBanks) + "\n";
    s += "So PRG banks 8KB  : " + std::to_string(prg8Count) + "\n";
    s += "So CHR banks 8KB  : " + std::to_string(nCHRBanks) + "\n";
    s += "So CHR banks 1KB  : " + std::to_string(chr1kCount) + "\n";
    s += "Mirroring         : " + mirrorToString(mirrorMode) + "\n";

    s += "\nPRG BANK HIEN TAI:\n";
    s += "$8000-$BFFF : PRG bank 16KB = " + std::to_string(prg16Bank) +
        " | offset ROM = 0x" + HexStr((prg16Bank % nPRGBanks) * 0x4000, 6) + "\n";

    s += "$C000-$DFFF : PRG bank 8KB  = " + std::to_string(prg8Bank) +
        " | offset ROM = 0x" + HexStr((prg8Bank % prg8Count) * 0x2000, 6) + "\n";

    s += "$E000-$FFFF : PRG bank 8KB cuoi = " + std::to_string(prg8Count - 1) +
        " | offset ROM = 0x" + HexStr((prg8Count - 1) * 0x2000, 6) + "\n";

    s += "\nCHR BANK HIEN TAI:\n";
    for (int i = 0; i < 8; i++)
    {
        uint16_t addr = i * 0x0400;
        uint8_t reg = GetChrRegisterForPpuAddress(addr);
        uint8_t realBank = GetChrBankForPpuAddress(addr);

        s += "$" + HexStr(addr, 4) + "-$" + HexStr(addr + 0x03FF, 4) +
            " : dung CHR[" + std::to_string(reg) + "]=" + std::to_string(chrBank[reg]) +
            " | bank thuc=" + std::to_string(realBank) +
            " | offset CHR=0x" + HexStr((realBank % chr1kCount) * 0x0400, 6) + "\n";
    }

    s += "\nNAMETABLE / MIRRORING:\n";
    s += "Thanh ghi $B003             : 0x" + HexStr(b003, 2) + "\n";
    s += "PPU Banking Mode            : " + std::to_string(ppuBankingMode) + "\n";
    s += std::string("Nametable dung CHR-ROM      : ") + (useChrRomNametables ? "BAT" : "TAT") + "\n";
    s += std::string("Dieu khien CHR A10          : ") + (chrA10Control ? "BAT" : "TAT") + "\n";
    s += std::string("PRG RAM                     : ") + (prgRamEnable ? "BAT" : "TAT") + "\n";

    s += "\nNguon Nametable:\n";
    for (int i = 0; i < 4; i++)
    {
        s += "Nametable " + std::to_string(i) + " : CIRAM source=" + std::to_string(ntSource[i]) +
            " | CHR-NT bank=" + std::to_string(ntChrBank[i]) + "\n";
    }

    s += "\nTHONG TIN IRQ:\n";
    s += std::string("IRQ Enable           : ") + (irqEnable ? "BAT" : "TAT") + "\n";
    s += std::string("IRQ Enable After ACK : ") + (irqEnableAfterAck ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Active           : ") + (irqActive ? "CO" : "KHONG") + "\n";
    s += std::string("IRQ Mode             : ") + (irqModeCycle ? "CPU Cycle" : "Scanline") + "\n";
    s += "IRQ Latch            : " + std::to_string(irqLatch) + "\n";
    s += "IRQ Counter          : " + std::to_string(irqCounter) + "\n";
    s += "IRQ Prescaler        : " + std::to_string(irqPrescaler) + "\n";

    s += "\nAM THANH MO RONG VRC6:\n";
    s += std::string("Mute VRC6 : ") + (muteVRC6 ? "CO" : "KHONG") + "\n";

    s += "\nVRC6 Pulse 1:\n";
    s += std::string("Enable=") + (pulse1.enable ? "BAT" : "TAT") +
        " | Volume=" + std::to_string(pulse1.volume) +
        " | Duty=" + std::to_string(pulse1.duty) +
        " | Mode=" + (pulse1.mode ? "Constant" : "Duty") +
        " | Period=" + std::to_string(pulse1.period) +
        " | Timer=" + std::to_string(pulse1.timer) +
        " | Output=" + std::to_string(pulse1.output) + "\n";

    s += "\nVRC6 Pulse 2:\n";
    s += std::string("Enable=") + (pulse2.enable ? "BAT" : "TAT") +
        " | Volume=" + std::to_string(pulse2.volume) +
        " | Duty=" + std::to_string(pulse2.duty) +
        " | Mode=" + (pulse2.mode ? "Constant" : "Duty") +
        " | Period=" + std::to_string(pulse2.period) +
        " | Timer=" + std::to_string(pulse2.timer) +
        " | Output=" + std::to_string(pulse2.output) + "\n";

    s += "\nVRC6 Saw:\n";
    s += std::string("Enable=") + (saw.enable ? "BAT" : "TAT") +
        " | Rate=" + std::to_string(saw.rate) +
        " | Period=" + std::to_string(saw.period) +
        " | Timer=" + std::to_string(saw.timer) +
        " | Step=" + std::to_string(saw.step) +
        " | Acc=" + std::to_string(saw.accumulator) +
        " | Output=" + std::to_string(saw.output) + "\n";

    return s;
}
uint8_t Mapper_024::GetNtSource(int index) const
{
    return ntSource[index & 0x03] & 0x01;
}