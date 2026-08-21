#include "Cartridge.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <iterator>
#include "BinaryIO.h"
#include "Mapper_000.h"
#include "Mapper_001.h"
#include "Mapper_002.h"
#include "Mapper_003.h"
#include "Mapper_004.h"
#include "Mapper_005.h"
#include "Mapper_007.h"
#include "Mapper_009.h"
#include "Mapper_011.h"
#include "Mapper_015.h"
#include "Mapper_018.h"
#include "Mapper_019.h"
#include "Mapper_021.h"
#include "Mapper_023.h"
#include "Mapper_024.h"
#include "Mapper_025.h"
#include "Mapper_026.h"
#include "Mapper_066.h"
#include "Mapper_068.h"
#include "Mapper_069.h"
#include "Mapper_070.h"
#include "Mapper_071.h"
#include "Mapper_075.h"
#include "Mapper_079.h"
#include "Mapper_085.h"
#include "Mapper_087.h"
#include "Mapper_185.h"
#include "Mapper_221.h"
#include "Mapper_476.h"
#include "Mapper_020.h"

std::vector<uint8_t> Cartridge::s_fdsBios;

bool Cartridge::LoadFDSBios(const std::string& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
    {
        std::cout << "Khong mo duoc FDS BIOS: " << path << std::endl;
        return false;
    }

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    if (data.size() != 8192)
    {
        std::cout << "CANH BAO: disksys.rom khong dung 8KB (thuc te: "
            << data.size() << " byte). Van nap, se tu pad/cat ve 8KB." << std::endl;
    }

    s_fdsBios = std::move(data);
    return true;
}

bool Cartridge::IsFDSBiosLoaded()
{
    return !s_fdsBios.empty();
}

Cartridge::Cartridge(const std::string& sFileName)
{
    LoadFromFile(sFileName);
}

Cartridge::Cartridge(const std::vector<uint8_t>& romData)
{
    LoadFromData(romData);
}

bool Cartridge::LoadFromFile(const std::string& sFileName)
{
    std::ifstream ifs(sFileName, std::ios::binary);

    if (!ifs.is_open())
    {
        bImageValid = false;
        return false;
    }

    std::vector<uint8_t> romData(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );

    ifs.close();

    return LoadFromData(romData);
}

bool Cartridge::LoadFromData(const std::vector<uint8_t>& romData)
{
    bImageValid = false;

    vPRGMemory.clear();
    vCHRMemory.clear();
    PRGRAM.clear();
    pMapper.reset();

    // ---- Phát hiện file .fds (FDS - Famicom Disk System) ----
    // Có 2 biến thể: có header 16 byte "FDS\x1A" + số side, hoặc headerless
    // (raw, size chia hết cho 65500 = kích thước 1 side theo chuẩn .fds).
    bool isFdsWithHeader = romData.size() >= 16 &&
        romData[0] == 'F' && romData[1] == 'D' && romData[2] == 'S' && romData[3] == 0x1A;
    bool isFdsHeaderless = !isFdsWithHeader &&
        romData.size() > 0 && (romData.size() % 65500 == 0);

    if (isFdsWithHeader || isFdsHeaderless)
    {
        size_t offset = isFdsWithHeader ? 16 : 0;
        size_t remaining = romData.size() - offset;
        size_t sideCount = remaining / 65500;

        if (sideCount == 0)
            return false;

        std::vector<std::vector<uint8_t>> sides;
        sides.reserve(sideCount);

        for (size_t i = 0; i < sideCount; i++)
        {
            std::vector<uint8_t> side(
                romData.begin() + offset + i * 65500,
                romData.begin() + offset + (i + 1) * 65500
            );
            sides.push_back(std::move(side));
        }

        nMapperID = 20;
        nPRGBanks = 0;
        nCHRBanks = 0;
        mirror = VERTICAL; // giá trị khởi tạo, game sẽ tự set qua $4025 bit3

        // FDS dùng CHR-RAM 8KB và PRG-RAM 32KB cố định, không có PRG/CHR-ROM
        // thật từ file (khác hẳn cartridge iNES thông thường).
        vCHRMemory.resize(8192, 0x00);
        PRGRAM.assign(64 * 1024, 0x00);

        auto m20 = std::make_shared<Mapper_020>(0, 0);
        m20->SetBIOS(s_fdsBios);
        m20->SetDiskSides(std::move(sides));
        pMapper = m20;

        std::cout << "FDS loaded: " << sideCount << " side(s)." << std::endl;
        if (s_fdsBios.empty())
            std::cout << "CANH BAO: chua nap BIOS (disksys.rom) - game se KHONG chay duoc! "
            "Goi Cartridge::LoadFDSBios() truoc khi mo game FDS." << std::endl;

        bImageValid = true;
        return true;
    }

    if (romData.size() < 16)
        return false;

    struct sHeader {
        char name[4];
        uint8_t prg_rom_chunks;
        uint8_t chr_rom_chunks;
        uint8_t mapper1;
        uint8_t mapper2;
        uint8_t prg_ram_size;
        uint8_t tv_system1;
        uint8_t tv_system2;
        char unused[5];
    } header;

    std::memcpy(&header, romData.data(), sizeof(sHeader));

    if (header.name[0] != 'N' || header.name[1] != 'E' ||
        header.name[2] != 'S' || header.name[3] != 0x1A)
    {
        return false;
    }

    size_t offset = 16;

    nMapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
    nMapperID |= (uint16_t)(header.prg_ram_size & 0x0F) << 8;

    if (header.mapper1 & 0x01)
        mirror = VERTICAL;
    else
        mirror = HORIZONTAL;

    if (header.mapper1 & 0x04)
        offset += 512;

    nPRGBanks = header.prg_rom_chunks;
    nCHRBanks = header.chr_rom_chunks;

    nPRGBanks = header.prg_rom_chunks;
    size_t prgSize = size_t(nPRGBanks) * 16384;
    size_t chrSize;

    if (nMapperID == 476)
    {
        size_t remainingSize = romData.size() - offset - prgSize;
        nCHRBanks = static_cast<uint32_t>(remainingSize / 14336);
        chrSize = remainingSize;
    }
    else
    {
        nCHRBanks = header.chr_rom_chunks;
        chrSize = size_t(nCHRBanks) * 8192;
    }

    vPRGMemory.resize(prgSize);
    std::memcpy(vPRGMemory.data(), romData.data() + offset, prgSize);
    offset += prgSize;

    if (nCHRBanks == 0)
    {
        vCHRMemory.resize(8192, 0x00);
    }
    else
    {
        if (romData.size() < offset + chrSize)
            return false;

        vCHRMemory.resize(chrSize);
        std::memcpy(vCHRMemory.data(), romData.data() + offset, chrSize);
        offset += chrSize;
    }

    if (nMapperID == 19)
    {
        vCHRMemory.resize(vCHRMemory.size() + 2048, 0x00);
    }

    PRGRAM.resize(64 * 1024, 0x00);

    switch (nMapperID) {
    case 0:   pMapper = std::make_shared<Mapper_000>(nPRGBanks, nCHRBanks); break;
    case 1:   pMapper = std::make_shared<Mapper_001>(nPRGBanks, nCHRBanks); break;
    case 2:   pMapper = std::make_shared<Mapper_002>(nPRGBanks, nCHRBanks); break;
    case 3:   pMapper = std::make_shared<Mapper_003>(nPRGBanks, nCHRBanks); break;
    case 4:   pMapper = std::make_shared<Mapper_004>(nPRGBanks, nCHRBanks); break;
    case 5:   pMapper = std::make_shared<Mapper_005>(nPRGBanks, nCHRBanks); break;
    case 7:   pMapper = std::make_shared<Mapper_007>(nPRGBanks, nCHRBanks); break;
    case 9:   pMapper = std::make_shared<Mapper_009>(nPRGBanks, nCHRBanks); break;
    case 11:  pMapper = std::make_shared<Mapper_011>(nPRGBanks, nCHRBanks); break;
    case 15:  pMapper = std::make_shared<Mapper_015>(nPRGBanks, nCHRBanks); break;
    case 18:  pMapper = std::make_shared<Mapper_018>(nPRGBanks, nCHRBanks); break;
    case 19:  pMapper = std::make_shared<Mapper_019>(nPRGBanks, nCHRBanks); break;
    case 21: pMapper = std::make_shared<Mapper_021>(nPRGBanks, nCHRBanks, VRC4Variant::VRC4a); break;
    case 23:  pMapper = std::make_shared<Mapper_023>(nPRGBanks, nCHRBanks); break;
    case 24:  pMapper = std::make_shared<Mapper_024>(nPRGBanks, nCHRBanks); break;
    case 25:  pMapper = std::make_shared<Mapper_025>(nPRGBanks, nCHRBanks); break;
    case 66:  pMapper = std::make_shared<Mapper_066>(nPRGBanks, nCHRBanks); break;
    case 68:  pMapper = std::make_shared<Mapper_068>(nPRGBanks, nCHRBanks); break;
    case 69:  pMapper = std::make_shared<Mapper_069>(nPRGBanks, nCHRBanks); break;
    case 71:  pMapper = std::make_shared<Mapper_071>(nPRGBanks, nCHRBanks); break;
    case 70:  pMapper = std::make_shared<Mapper_070>(nPRGBanks, nCHRBanks); break;
    case 75:  pMapper = std::make_shared<Mapper_075>(nPRGBanks, nCHRBanks); break;
    case 79:  pMapper = std::make_shared<Mapper_079>(nPRGBanks, nCHRBanks); break;
    case 85:  pMapper = std::make_shared<Mapper_085>(nPRGBanks, nCHRBanks); break;
    case 87:  pMapper = std::make_shared<Mapper_087>(nPRGBanks, nCHRBanks); break;
    case 185: pMapper = std::make_shared<Mapper_185>(nPRGBanks, nCHRBanks); break;
    case 221: pMapper = std::make_shared<Mapper_221>(nPRGBanks, nCHRBanks); break;
    case 476: pMapper = std::make_shared<Mapper_476>(nPRGBanks, nCHRBanks); break;
    case 26:  pMapper = std::make_shared<Mapper_026>(nPRGBanks, nCHRBanks); break;
    default:
        std::cout << "CHƯA HỖ TRỢ MAPPER ID: " << (int)nMapperID << std::endl;
        break;
    }

    if (nMapperID == 476)
    {
        if (auto* m476 = dynamic_cast<Mapper_476*>(pMapper.get()))
        {
            size_t frameSize = 4096 + 1024;
            size_t frameCount = vCHRMemory.size() / frameSize;
            std::vector<uint8_t> nametableData(frameCount * 1024);
            std::vector<uint8_t> patternOnly(frameCount * 4096);

            for (size_t f = 0; f < frameCount; f++)
            {
                std::memcpy(patternOnly.data() + f * 4096, vCHRMemory.data() + f * frameSize, 4096);
                std::memcpy(nametableData.data() + f * 1024, vCHRMemory.data() + f * frameSize + 4096, 1024);
            }

            vCHRMemory = std::move(patternOnly);
            m476->SetNametableData(std::move(nametableData));
        }
    }
    if (pMapper && (header.mapper2 & 0x0C) == 0x08)
    {
        pMapper->nSubmapper = (header.prg_ram_size >> 4);
    }

    std::cout << "Mapper ID: " << nMapperID << std::endl;
    std::cout << "PRG Banks: " << nPRGBanks << std::endl;
    std::cout << "CHR Banks: " << nCHRBanks << std::endl;

    bImageValid = (pMapper != nullptr);
    return bImageValid;
}

Cartridge::~Cartridge() {}

bool Cartridge::ImageValid()
{
    return bImageValid;
}

// 4 CỔNG GIAO TIẾP VỚI CPU VÀ PPU (Nhờ Mapper phiên dịch địa chỉ)

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data)
{
    if (pMapper && pMapper->cpuReadRegister(addr, data))
    {
        return true;
    }

    // ---- FDS (Mapper 20): sở hữu trọn $6000-$FFFF, xử lý hoàn toàn riêng ----
    if (nMapperID == 20)
    {
        if (auto* fds = dynamic_cast<Mapper_020*>(pMapper.get()))
        {
            if (addr >= 0xE000)
            {
                data = fds->ReadBios(addr);
                return true;
            }

            if (addr >= 0x6000 && addr <= 0xDFFF)
            {
                uint32_t off = uint32_t(addr) - 0x6000;
                data = (off < PRGRAM.size()) ? PRGRAM[off] : 0x00;
                return true;
            }

            // addr khác (vd $4000-$401F APU/IO) không thuộc FDS -> để Bus tự xử lý
            return false;
        }
    }

    // PRG RAM / WRAM $6000-$7FFF
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        // Mapper 69 / Sunsoft 5B: $6000-$7FFF có thể là PRG-ROM bank hoặc PRG-RAM
        if (auto* m69 = dynamic_cast<Mapper_069*>(pMapper.get()))
        {
            // Nếu mapper đang chọn ROM ở $6000 thì cho mapper map vào PRG-ROM
            if (!m69->IsPrg6000RamSelected())
            {
                uint32_t mapped_addr = 0;

                if (pMapper->cpuMapRead(addr, mapped_addr))
                {
                    if (mapped_addr < vPRGMemory.size())
                        data = vPRGMemory[mapped_addr];
                    else
                        data = 0x00;

                    return true;
                }
            }

            // Nếu chọn RAM nhưng RAM chưa enable thì trả 0
            if (!m69->IsPrg6000RamEnabled())
            {
                data = 0x00;
                return true;
            }

            uint32_t ramOffset = addr & 0x1FFF;

            if (ramOffset < PRGRAM.size())
                data = PRGRAM[ramOffset];
            else
                data = 0x00;

            return true;
        }

        // Các mapper khác giữ logic cũ
        uint32_t ramOffset = addr & 0x1FFF;

        if (pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(pMapper.get()))
            {
                ramOffset += uint32_t(mmc5->GetPrgRamBank()) * 0x2000;
            }
        }

        if (ramOffset < PRGRAM.size())
            data = PRGRAM[ramOffset];
        else
            data = 0x00;

        return true;
    }

    uint32_t mapped_addr = 0;

    if (pMapper && pMapper->cpuMapRead(addr, mapped_addr))
    {
        if (mapped_addr < vPRGMemory.size())
        {
            data = vPRGMemory[mapped_addr];
            return true;
        }

        data = 0x00;
        return true;
    }

    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data)
{

    if (pMapper == nullptr)
        return false;

    // ---- FDS (Mapper 20) ----
    if (nMapperID == 20)
    {
        if (auto* fds = dynamic_cast<Mapper_020*>(pMapper.get()))
        {
            if (addr >= 0x6000 && addr <= 0xDFFF)
            {
                uint32_t off = uint32_t(addr) - 0x6000;
                if (off < PRGRAM.size())
                    PRGRAM[off] = data;
                return true;
            }

            if (addr >= 0xE000)
            {
                // BIOS chỉ đọc -> nuốt write, tránh rơi xuống cpuMapWrite phía dưới
                return true;
            }

            // Còn lại ($4020-$4026 thanh ghi FDS) rơi tiếp xuống cpuMapWrite
            // ở cuối hàm (Mapper_020::cpuMapWrite xử lý).
        }
    }

    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        // Mapper 69: chỉ ghi RAM khi đang chọn RAM và RAM enabled
        if (auto* m69 = dynamic_cast<Mapper_069*>(pMapper.get()))
        {
            if (!m69->IsPrg6000RamSelected() || !m69->IsPrg6000RamEnabled())
                return true;

            uint32_t ramOffset = addr & 0x1FFF;

            if (ramOffset < PRGRAM.size())
                PRGRAM[ramOffset] = data;

            return true;
        }
        uint32_t ramOffset = addr & 0x1FFF;

        if (pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(pMapper.get()))
            {
                ramOffset += uint32_t(mmc5->GetPrgRamBank()) * 0x2000;
            }
        }

        if (ramOffset < PRGRAM.size())
            PRGRAM[ramOffset] = data;

        return true;
    }

    uint32_t mapped_addr = 0;

    if (pMapper->cpuMapWrite(addr, mapped_addr, data))
    {
        if (auto* m69 = dynamic_cast<Mapper_069*>(pMapper.get()))
        {
            int m = m69->GetMirrorMode();

            if (m == 0)
                mirror = VERTICAL;
            else if (m == 1)
                mirror = HORIZONTAL;
            else if (m == 2)
                mirror = ONESCREEN_LO;
            else if (m == 3)
                mirror = ONESCREEN_HI;
        }
        else
        {
            MIRROR mapperMirror = pMapper->mirror();

            if (mapperMirror != HARDWARE)
                mirror = mapperMirror;
        }

        return true;
    }

    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data)
{
    uint32_t mapped_addr = 0;

    if (pMapper && pMapper->ppuMapRead(addr, mapped_addr))
    {
        // Special marker: mapper wants PPU internal nametable, not CHR ROM
        if (mapped_addr & 0x80000000)
        {
            return false;
        }

        if (mapped_addr < vCHRMemory.size())
        {
            data = vCHRMemory[mapped_addr];
            return true;
        }

        data = 0x00;
        return true;
    }

    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data)
{
    uint32_t mapped_addr = 0;

    if (pMapper == nullptr)
        return false;

    if (pMapper->ppuMapWrite(addr, mapped_addr))
    {
        if (mapped_addr & 0x80000000)
        {
            return false;
        }

        if (mapped_addr < vCHRMemory.size())
        {
            vCHRMemory[mapped_addr] = data;
        }

        return true;
    }

    return false;
}
void Cartridge::SaveState(BinaryWriter& out) const
{
    out << mirror;
    uint32_t prgRamSize = static_cast<uint32_t>(PRGRAM.size());
    out << prgRamSize;
    for (uint8_t value : PRGRAM)
        out << value;
    uint32_t chrMemorySize = static_cast<uint32_t>(vCHRMemory.size());
    out << chrMemorySize;
    for (uint8_t value : vCHRMemory)
        out << value;
    if (pMapper)
        pMapper->SaveState(out);
}

void Cartridge::LoadState(BinaryReader& in)
{
    in >> mirror;
    uint32_t prgRamSize = 0;
    in >> prgRamSize;

    if (!in.ok)
        return;
    PRGRAM.resize(prgRamSize);
    for (uint8_t& value : PRGRAM)
    {
        in >> value;

        if (!in.ok)
            return;
    }
    uint32_t chrMemorySize = 0;
    in >> chrMemorySize;

    if (!in.ok)
        return;
    vCHRMemory.resize(chrMemorySize);

    for (uint8_t& value : vCHRMemory)
    {
        in >> value;

        if (!in.ok)
            return;
    }
    if (pMapper)
        pMapper->LoadState(in);
}