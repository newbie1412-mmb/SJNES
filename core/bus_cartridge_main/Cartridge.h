#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "Mapper.h"
#include "BinaryIO.h"
class BinaryWriter;
class BinaryReader;

class Cartridge {
public:
    ~Cartridge();

    Cartridge(const std::string& sFileName);
    Cartridge(const std::vector<uint8_t>& romData);

    bool LoadFromFile(const std::string& sFileName);
    bool LoadFromData(const std::vector<uint8_t>& romData);
    bool ImageValid();
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);
    MIRROR mirror = MIRROR::HORIZONTAL;
    std::vector<uint8_t> PRGRAM;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;

    uint16_t nMapperID = 0;
    uint32_t nPRGBanks = 0;
    uint32_t nCHRBanks = 0;

    std::shared_ptr<Mapper> pMapper;
    Mapper* GetMapper()
    {
        return pMapper.get();
    }
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);

    // ---- FDS (Mapper 20) ----
    // BIOS disksys.rom là phần cứng của máy FDS, KHÔNG nằm trong file .fds của
    // từng game, nên nạp 1 lần dùng chung cho mọi Cartridge (static), thay vì
    // per-instance. Gọi hàm này 1 lần lúc khởi động app, trước khi load game
    // FDS đầu tiên (ví dụ trong SJNES constructor).
    static bool LoadFDSBios(const std::string& path);
    static bool IsFDSBiosLoaded();
private:
    bool bImageValid = false;
    static std::vector<uint8_t> s_fdsBios;
};