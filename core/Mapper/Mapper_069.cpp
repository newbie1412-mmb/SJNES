#include "Mapper_069.h"

Mapper_069::Mapper_069(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

void Mapper_069::reset()
{
    command = 0x00;

    for (int i = 0; i < 8; i++)
        chrBank[i] = i;

    // PRG 8KB banks
    prgBank[0] = 0;
    prgBank[1] = 1;
    prgBank[2] = 2;
    prgBank[3] = 0xFF; // fixed last bank ở $E000-$FFFF

    mirrorMode = 0;

    irqEnabled = false;
    irqCounterEnabled = false;
    irqPending = false;
    irqCounter = 0;
    audioRegSelect = 0;
    audioDivider = 0;
    prg6000RamSelected = false;
    prg6000RamEnabled = false;
    for (int i = 0; i < 16; i++)
        audioRegs[i] = 0;

    for (int i = 0; i < 3; i++)
    {
        toneCounter[i] = 0;
        toneOutput[i] = false;
        lastToneSample[i] = 0.0f;
    }
}

bool Mapper_069::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    uint32_t total8KBanks = uint32_t(nPRGBanks) * 2;
    if (total8KBanks == 0)
        total8KBanks = 1;

    // $6000-$7FFF: Mapper 69 có thể map PRG-ROM hoặc PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        // Nếu đang chọn RAM thì Cartridge sẽ xử lý PRGRAM
        if (prg6000RamSelected)
            return false;

        // Nếu không chọn RAM thì đây là PRG-ROM bank
        uint32_t bank = prgBank[0] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr < 0x8000)
        return false;

    uint32_t bank = 0;

    if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        bank = prgBank[1] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        bank = prgBank[2] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        bank = prgBank[3] % total8KBanks;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xE000)
    {
        bank = total8KBanks - 1;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper_069::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    // Sunsoft 5B audio register select
    if (addr >= 0xC000 && addr <= 0xDFFF)
    {
        audioRegSelect = data & 0x0F;
        mapped_addr = 0;
        return true;
    }

    // Sunsoft 5B audio register write
    if (addr >= 0xE000 && addr <= 0xFFFF)
    {
        audioRegs[audioRegSelect] = data;
        mapped_addr = 0;
        return true;
    }
    if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        command = data & 0x0F;
        mapped_addr = 0;
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        switch (command)
        {
        case 0x0: chrBank[0] = data; break;
        case 0x1: chrBank[1] = data; break;
        case 0x2: chrBank[2] = data; break;
        case 0x3: chrBank[3] = data; break;
        case 0x4: chrBank[4] = data; break;
        case 0x5: chrBank[5] = data; break;
        case 0x6: chrBank[6] = data; break;
        case 0x7: chrBank[7] = data; break;

        case 0x8:
            prgBank[0] = data & 0x3F;

            // FME-7/Sunsoft 5B $6000-$7FFF control
            // bit 6: chọn RAM thay vì ROM
            // bit 7: enable RAM
            prg6000RamSelected = (data & 0x40) != 0;
            prg6000RamEnabled = (data & 0x80) != 0;
            break;

        case 0x9:
            prgBank[1] = data & 0x3F;
            break;

        case 0xA:
            prgBank[2] = data & 0x3F;
            break;

        case 0xB:
            prgBank[3] = data & 0x3F;
            break;

        case 0xC:
            mirrorMode = data & 0x03;
            break;

        case 0xD:
            irqEnabled = (data & 0x01) != 0;
            irqCounterEnabled = (data & 0x80) != 0;
            irqPending = false;
            break;

        case 0xE:
            irqCounter = (irqCounter & 0xFF00) | data;
            break;

        case 0xF:
            irqCounter = (irqCounter & 0x00FF) | (uint16_t(data) << 8);
            break;
        }

        mapped_addr = 0;
        return true;
    }

    return false;
}

bool Mapper_069::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        uint32_t total1KBanks = uint32_t(nCHRBanks) * 8;

        // Nếu CHR-RAM
        if (total1KBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        uint8_t slot = addr / 0x0400;
        uint32_t bank = chrBank[slot] % total1KBanks;

        mapped_addr = bank * 0x0400 + (addr & 0x03FF);
        return true;
    }

    return false;
}

bool Mapper_069::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        // CHR-RAM case
        if (nCHRBanks == 0)
        {
            mapped_addr = addr & 0x1FFF;
            return true;
        }

        return false;
    }

    return false;
}

void Mapper_069::irqStep()
{
    audioDivider++;

    if (audioDivider >= 16)
    {
        audioDivider = 0;

        for (int ch = 0; ch < 3; ch++)
        {
            int fineReg = ch * 2;
            int coarseReg = ch * 2 + 1;

            int period = audioRegs[fineReg] | ((audioRegs[coarseReg] & 0x0F) << 8);

            if (period <= 0)
                period = 1;

            if (toneCounter[ch] <= 0)
            {
                toneCounter[ch] = period;
                toneOutput[ch] = !toneOutput[ch];
            }
            else
            {
                toneCounter[ch]--;
            }
        }
    }

    for (int ch = 0; ch < 3; ch++)
    {
        uint8_t mixer = audioRegs[7];
        bool toneDisabled = (mixer & (1 << ch)) != 0;

        uint8_t volReg = audioRegs[8 + ch];
        int volume = volReg & 0x0F;

        if (toneDisabled || volume == 0)
        {
            lastToneSample[ch] = 0.0f;
        }
        else
        {
            float amp = float(volume) / 15.0f;

            lastToneSample[ch] = toneOutput[ch] ? amp : -amp;
        }
    }

   
    if (irqCounterEnabled)
    {
        uint16_t old = irqCounter;
        irqCounter--;

        if (old == 0x0000 && irqEnabled)
        {
            irqPending = true;
        }
    }
}

bool Mapper_069::irqState()
{
    return irqPending;
}

void Mapper_069::irqClear()
{
    irqPending = false;
}

float Mapper_069::GetExpansionAudio()
{
    float mix = lastToneSample[0] + lastToneSample[1] + lastToneSample[2];
    return mix * 0.07f;
}

void Mapper_069::GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3)
{
    ch1 = lastToneSample[0];
    ch2 = lastToneSample[1];
    ch3 = lastToneSample[2];
}

void Mapper_069::GetExpansionAudioStereo(float& expL, float& expR)
{
    float mix = GetExpansionAudio();
    expL = mix;
    expR = mix;
}

void Mapper_069::GetS5BDebugPeriods(float& ch1, float& ch2, float& ch3) const
{
    constexpr float CPU_TO_SAMPLE = 44100.0f / 1789773.0f;
    auto calc = [&](int ch) -> float {
        uint8_t mixer = audioRegs[7];
        bool toneDisabled = (mixer & (1 << ch)) != 0;
        uint8_t volReg = audioRegs[8 + ch];
        int volume = volReg & 0x0F;

        if (toneDisabled || volume == 0)
            return 0.0f;

        int fineReg = ch * 2;
        int coarseReg = ch * 2 + 1;
        int period = audioRegs[fineReg] | ((audioRegs[coarseReg] & 0x0F) << 8);
        if (period <= 0)
            period = 1;

        // Tone output đảo trạng thái sau mỗi period qua divider 16 CPU cycle.
        // Một chu kỳ vuông cần 2 lần đảo.
        float samples = float(period + 1) * 32.0f * CPU_TO_SAMPLE;
        if (samples < 2.0f) return 0.0f;
        if (samples > 8192.0f) return 8192.0f;
        return samples;
        };

    ch1 = calc(0);
    ch2 = calc(1);
    ch3 = calc(2);
}

void Mapper_069::GetS5BDebugDuty(float& ch1, float& ch2, float& ch3) const
{
    // PSG/Sunsoft 5B tone là square 50%.
    auto active = [&](int ch) -> float {
        uint8_t mixer = audioRegs[7];
        bool toneDisabled = (mixer & (1 << ch)) != 0;
        uint8_t volReg = audioRegs[8 + ch];
        int volume = volReg & 0x0F;
        return (!toneDisabled && volume > 0) ? 0.5f : -1.0f;
    };

    ch1 = active(0);
    ch2 = active(1);
    ch3 = active(2);
}

std::string Mapper_069::GetDebugInfo()
{
    std::string s;

    s += "===== Sunsoft FME-7 / 5B Mapper 69 =====\n";
    s += "Command: " + std::to_string(command) + "\n";
    s += "Mirror Mode: " + std::to_string(mirrorMode) + "\n\n";

    s += "CHR Banks 1KB:\n";
    for (int i = 0; i < 8; i++)
    {
        s += "CHR[" + std::to_string(i) + "] = " + std::to_string(chrBank[i]) + "\n";
    }

    s += "\nPRG Banks 8KB:\n";
    s += "$6000: " + std::to_string(prgBank[0]) + "  (tam chua map trong Cartridge)\n";
    s += "$8000: " + std::to_string(prgBank[1]) + "\n";
    s += "$A000: " + std::to_string(prgBank[2]) + "\n";
    s += "$C000: " + std::to_string(prgBank[3]) + "\n";
    s += "$E000: fixed last\n\n";

    s += "IRQ Enabled: " + std::to_string(irqEnabled) + "\n";
    s += "IRQ Counter Enabled: " + std::to_string(irqCounterEnabled) + "\n";
    s += "IRQ Pending: " + std::to_string(irqPending) + "\n";
    s += "IRQ Counter: " + std::to_string(irqCounter) + "\n";

    return s;
}