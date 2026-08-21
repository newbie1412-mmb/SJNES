#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

class BinaryWriter;
class BinaryReader;

enum MIRROR {
    HARDWARE,
    HORIZONTAL,
    VERTICAL,
    ONESCREEN_LO,
    ONESCREEN_HI,
};

inline std::string HexStr(uint32_t value, int width) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return oss.str();
}

class Mapper {
public:
    Mapper(uint32_t prgBanks, uint32_t chrBanks);
    virtual ~Mapper();

    // === CÁC HÀM QUẢN LÝ IRQ MỚI (Cho Namco 163 / Mapper 019) ===
    virtual void clock() {}
    virtual bool irqState() { return false; }
    virtual void irqClear() {}

    // === CÁC HÀM CŨ PHẢI GIỮ LẠI (Cho MMC3, MMC5, VRC6...) ===
    virtual MIRROR mirror() { return HARDWARE; }
    virtual void ClockA12() {}
    virtual void irqStep() {}
    virtual float GetExpansionAudio() { return 0.0f; }

    virtual void GetExpansionAudioStereo(float& left, float& right) {
        float v = GetExpansionAudio();
        left = v;
        right = v;
    }

    virtual bool cpuReadRegister(uint16_t addr, uint8_t& data) { return false; }
    virtual void GetExpansionDebugChannels(float& ch1, float& ch2, float& ch3) { ch1 = 0.0f; ch2 = 0.0f; ch3 = 0.0f; }

    virtual void ClockCpu(int cycles) {}
    virtual float GetExpansionAudioSample() const { return 0.0f; }

    virtual std::string GetDebugInfo() {
        std::string s = "===== DEBUG MAPPER =====\n";
        s += "PRG Banks: " + std::to_string(nPRGBanks) + "\n";
        s += "CHR Banks: " + std::to_string(nCHRBanks) + "\n";
        s += "Submapper: " + std::to_string(nSubmapper) + "\n";
        return s;
    }

    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() = 0;

    // === SAVE/LOAD STATE (Default: empty, override nếu cần) ===
    virtual void SaveState(BinaryWriter& out) const {}
    virtual void LoadState(BinaryReader& in) {}

    uint32_t nPRGBanks = 0;
    uint32_t nCHRBanks = 0;
    uint8_t nSubmapper = 0;
};