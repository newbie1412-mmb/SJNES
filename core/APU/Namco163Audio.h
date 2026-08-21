#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

class Namco163Audio
{
public:
    void Reset()
    {
        debugOut.fill(0.0f);
        ram.fill(0);
        address = 0;
        autoIncrement = false;

        soundDisabled = true;

        cpuCycleCounter = 0;
        currentChannel = 7;
        heldOutput = 0;
        volume = 2.00f;
        accurateMultiplex = false;
    }
    void GetDebugChannels(float out[8]) const
    {
        for (int i = 0; i < 8; i++)
            out[i] = 0.0f;

        if (soundDisabled)
            return;

        int count = GetActiveChannelCount();
        int lowest = GetLowestEnabledChannel();

        // N163 active channel là từ lowest..7
        // Nhưng để cửa sổ dễ nhìn, dồn về dòng 0..count-1
        for (int ch = lowest; ch < 8; ch++)
        {
            int viewIndex = ch - lowest;

            if (viewIndex >= 0 && viewIndex < 8)
            {
                int raw = ReadChannelOutput(ch);
                out[viewIndex] = NormalizeOutput(raw) * volume;
            }
        }
    }
    void GetDebugPeriods(float out[8]) const
    {
        for (int i = 0; i < 8; i++)
            out[i] = 0.0f;

        if (soundDisabled)
            return;

        constexpr float CPU_RATE = 1789773.0f;
        constexpr float SAMPLE_RATE = 44100.0f;

        int count = GetActiveChannelCount();
        int lowest = GetLowestEnabledChannel();

        for (int ch = lowest; ch < 8; ch++)
        {
            int viewIndex = ch - lowest;
            if (viewIndex < 0 || viewIndex >= 8)
                continue;

            int base = 0x40 + ch * 8;

            uint32_t freq =
                (uint32_t(ram[base + 4] & 0x03) << 16) |
                (uint32_t(ram[base + 2]) << 8) |
                uint32_t(ram[base + 0]);

            uint32_t waveLength = 256 - (ram[base + 4] & 0xFC);
            if (waveLength == 0)
                waveLength = 256;

            if (freq == 0)
                continue;

            // Mỗi channel được clock 1 lần sau 15 * số-kênh CPU cycle.
            // Một vòng wave cần (waveLength << 16) / freq lần update phase.
            float channelCpuStep = 15.0f * float(count);
            float updatesPerWave = (float(waveLength) * 65536.0f) / float(freq);
            float cpuCyclesPerWave = updatesPerWave * channelCpuStep;
            float periodSamples = cpuCyclesPerWave * SAMPLE_RATE / CPU_RATE;

            out[viewIndex] = std::clamp(periodSamples, 2.0f, 8192.0f);
        }
    }

    void SetVolume(float v)
    {
        volume = v;
    }

    void SetAccurateMultiplex(bool enabled)
    {
        accurateMultiplex = enabled;
    }

    bool IsSoundEnabled() const
    {
        return !soundDisabled;
    }

    // $E000-$E7FF
    // bit 6 = 1 thì tắt sound
    void WriteSoundEnable(uint8_t data)
    {
        soundDisabled = (data & 0x40) != 0;
    }

    // $F800-$FFFF
    // bit 7 = auto increment
    // bit 0-6 = địa chỉ RAM trong chip
    void WriteAddressPort(uint8_t data)
    {
        address = data & 0x7F;
        autoIncrement = (data & 0x80) != 0;
    }

    // $4800-$4FFF write
    void WriteDataPort(uint8_t data)
    {
        ram[address] = data;

        if (autoIncrement)
            address = (address + 1) & 0x7F;
    }

    // $4800-$4FFF read
    uint8_t ReadDataPort()
    {
        uint8_t value = ram[address];

        if (autoIncrement)
            address = (address + 1) & 0x7F;

        return value;
    }

    // Gọi theo CPU cycle.
    // Ví dụ CPU chạy xong opcode mất 4 cycle thì gọi ClockCpu(4)
    void ClockCpu(int cpuCycles)
    {
        if (soundDisabled)
            return;

        cpuCycleCounter += cpuCycles;

        while (cpuCycleCounter >= 15)
        {
            cpuCycleCounter -= 15;
            ClockOneChannel();
        }
    }

    float GetSample() const
    {
        if (soundDisabled)
            return 0.0f;

        if (accurateMultiplex)
        {
            return NormalizeOutput(heldOutput) * volume;
        }

        return GetMixedSampleApprox();
    }

    uint8_t PeekRam(uint8_t addr) const
    {
        return ram[addr & 0x7F];
    }

    int GetActiveChannelCount() const
    {
        // Register $7F bits 4-6:
        // C = 0 => 1 kênh
        // C = 1 => 2 kênh
        // ...
        // C = 7 => 8 kênh
        return ((ram[0x7F] >> 4) & 0x07) + 1;
    }

private:
    std::array<float, 8> debugOut{};
    std::array<uint8_t, 0x80> ram{};

    uint8_t address = 0;
    bool autoIncrement = false;
    bool soundDisabled = true;

    int cpuCycleCounter = 0;
    int currentChannel = 7;
    int heldOutput = 0;

    float volume = 0.35f;
    bool accurateMultiplex = false;

private:
    static float NormalizeOutput(int value)
    {
        // sample 4-bit: 0..15
        // sample - 8 => -8..7
        // volume: 0..15
        // max gần đúng: +-120
        return std::clamp(value / 120.0f, -1.0f, 1.0f);
    }

    uint8_t ReadWaveSample(uint8_t sampleIndex) const
    {
        // N163 lưu 2 sample 4-bit trong 1 byte:
        // sample chẵn = low nibble
        // sample lẻ = high nibble
        uint8_t byte = ram[(sampleIndex >> 1) & 0x7F];

        if (sampleIndex & 1)
            return (byte >> 4) & 0x0F;

        return byte & 0x0F;
    }

    int GetLowestEnabledChannel() const
    {
        int count = GetActiveChannelCount();

        // enabled từ kênh 8 lùi xuống:
        // 1 kênh: ch 7
        // 2 kênh: ch 7,6
        // ...
        // 8 kênh: ch 7..0
        return 8 - count;
    }

    int ReadChannelOutput(int ch) const
    {
        int base = 0x40 + ch * 8;

        uint32_t phase =
            (uint32_t(ram[base + 5]) << 16) |
            (uint32_t(ram[base + 3]) << 8) |
            uint32_t(ram[base + 1]);

        uint8_t lengthByte = ram[base + 4];
        uint32_t waveLength = 256 - (lengthByte & 0xFC);

        if (waveLength == 0)
            waveLength = 256;

        uint8_t waveAddress = ram[base + 6];
        uint8_t volume4 = ram[base + 7] & 0x0F;

        uint8_t sampleIndex = uint8_t(((phase >> 16) + waveAddress) & 0xFF);
        uint8_t sample = ReadWaveSample(sampleIndex);

        return (int(sample) - 8) * int(volume4);
    }

    void ClockOneChannel()
    {
        int lowest = GetLowestEnabledChannel();

        for (int i = 0; i < lowest; i++)
            debugOut[i] = 0.0f;

        if (currentChannel < lowest || currentChannel > 7)
            currentChannel = 7;

        int ch = currentChannel;
        int base = 0x40 + ch * 8;

        uint32_t phase =
            (uint32_t(ram[base + 5]) << 16) |
            (uint32_t(ram[base + 3]) << 8) |
            uint32_t(ram[base + 1]);

        uint32_t freq =
            (uint32_t(ram[base + 4] & 0x03) << 16) |
            (uint32_t(ram[base + 2]) << 8) |
            uint32_t(ram[base + 0]);

        uint32_t waveLength = 256 - (ram[base + 4] & 0xFC);

        if (waveLength == 0)
            waveLength = 256;

        uint32_t modulo = waveLength << 16;

        phase += freq;

        if (modulo != 0)
            phase %= modulo;

        ram[base + 1] = uint8_t(phase & 0xFF);
        ram[base + 3] = uint8_t((phase >> 8) & 0xFF);
        ram[base + 5] = uint8_t((phase >> 16) & 0xFF);

        heldOutput = ReadChannelOutput(ch);

        currentChannel--;

        if (currentChannel < lowest)
            currentChannel = 7;
    }

    float GetMixedSampleApprox() const
    {
        int count = GetActiveChannelCount();
        int lowest = GetLowestEnabledChannel();

        int sum = 0;

        for (int ch = lowest; ch < 8; ch++)
        {
            sum += ReadChannelOutput(ch);
        }

        if (count > 0)
            sum /= count;

        return NormalizeOutput(sum) * volume;
    }
};