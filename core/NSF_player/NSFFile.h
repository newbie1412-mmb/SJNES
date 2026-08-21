#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct NSFInfo
{
    uint8_t version = 0;
    uint8_t totalSongs = 0;
    uint8_t startingSong = 0;

    uint16_t loadAddress = 0;
    uint16_t initAddress = 0;
    uint16_t playAddress = 0;

    std::string songName;
    std::string artist;
    std::string copyright;

    uint16_t ntscSpeed = 0;
    uint16_t palSpeed = 0;

    uint8_t region = 0;
    uint8_t expansion = 0;

    uint8_t bankSwitch[8] = {};
};

class NSFFile
{
public:
    // Core không tự đọc file nữa — nhận buffer thô đã đọc sẵn từ bên ngoài (UI)
    bool LoadFromBuffer(const std::vector<uint8_t>& data, std::string* error = nullptr);

    const NSFInfo& Info() const { return info; }
    const std::vector<uint8_t>& ProgramData() const { return programData; }

    bool UsesBankSwitching() const;
    bool UsesVRC6() const;
    bool UsesVRC7() const;
    bool UsesFDS() const;
    bool UsesMMC5() const;
    bool UsesN163() const;
    bool UsesS5B() const;

private:
    static uint16_t ReadLE16(const std::vector<uint8_t>& data, int offset);
    static std::string ReadText32(const std::vector<uint8_t>& data, int offset);

private:
    NSFInfo info;
    std::vector<uint8_t> programData;
};