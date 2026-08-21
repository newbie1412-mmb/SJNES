#include "NSFFile.h"
#include <cstring>

uint16_t NSFFile::ReadLE16(const std::vector<uint8_t>& data, int offset)
{
    uint8_t lo = data[offset];
    uint8_t hi = data[offset + 1];
    return static_cast<uint16_t>(lo | (hi << 8));
}

std::string NSFFile::ReadText32(const std::vector<uint8_t>& data, int offset)
{
    std::string text(reinterpret_cast<const char*>(data.data() + offset), 32);

    size_t zeroIndex = text.find('\0');
    if (zeroIndex != std::string::npos)
        text = text.substr(0, zeroIndex);

    // trim khoảng trắng đầu/cuối (thay cho QString::trimmed())
    size_t start = text.find_first_not_of(" \t\r\n");
    size_t end = text.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return text.substr(start, end - start + 1);
}

bool NSFFile::LoadFromBuffer(const std::vector<uint8_t>& data, std::string* error)
{
    if (data.size() < 0x80)
    {
        if (error) *error = "File NSF qua nho.";
        return false;
    }

    if (!(data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 'M' && data[4] == 0x1A))
    {
        if (error) *error = "Sai header NSF. File khong phai NSF hop le.";
        return false;
    }

    info = NSFInfo{};
    programData.clear();

    info.version = data[0x05];
    info.totalSongs = data[0x06];
    info.startingSong = data[0x07];

    info.loadAddress = ReadLE16(data, 0x08);
    info.initAddress = ReadLE16(data, 0x0A);
    info.playAddress = ReadLE16(data, 0x0C);

    info.songName = ReadText32(data, 0x0E);
    info.artist = ReadText32(data, 0x2E);
    info.copyright = ReadText32(data, 0x4E);

    info.ntscSpeed = ReadLE16(data, 0x6E);

    for (int i = 0; i < 8; i++)
        info.bankSwitch[i] = data[0x70 + i];

    info.palSpeed = ReadLE16(data, 0x78);
    info.region = data[0x7A];
    info.expansion = data[0x7B];

    const int programOffset = 0x80;
    programData.assign(data.begin() + programOffset, data.end());

    if (programData.empty())
    {
        if (error) *error = "NSF khong co program data.";
        return false;
    }

    return true;
}

bool NSFFile::UsesBankSwitching() const
{
    for (int i = 0; i < 8; i++)
        if (info.bankSwitch[i] != 0)
            return true;
    return false;
}

bool NSFFile::UsesVRC6() const { return (info.expansion & 0x01) != 0; }
bool NSFFile::UsesVRC7() const { return (info.expansion & 0x02) != 0; }
bool NSFFile::UsesFDS()  const { return (info.expansion & 0x04) != 0; }
bool NSFFile::UsesMMC5() const { return (info.expansion & 0x08) != 0; }
bool NSFFile::UsesN163() const { return (info.expansion & 0x10) != 0; }
bool NSFFile::UsesS5B()  const { return (info.expansion & 0x20) != 0; }