#include "Mapper_075.h"
#include <iostream>

Mapper_075::Mapper_075(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks)
{
    reset();
}

Mapper_075::~Mapper_075() {}

void Mapper_075::reset()
{
    uint8_t total8K = nPRGBanks * 2;

    prg_bank[0] = 0;
    prg_bank[1] = (total8K > 1) ? 1 : 0;
    prg_bank[2] = (total8K > 2) ? 2 : 0;
    prg_bank[3] = total8K - 1;

    chr_bank[0] = 0;
    chr_bank[1] = 1;

    chr_bit_high_0 = 0;
    chr_bit_high_1 = 0;

    mirroring_mode = 0;
}
bool Mapper_075::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        mapped_addr = prg_bank[0] * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        mapped_addr = prg_bank[1] * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    else if (addr >= 0xC000 && addr <= 0xDFFF) {
        mapped_addr = prg_bank[2] * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        // Bank cuối cùng cố định vào vị trí bank PRG cuối cùng (8KB cuối)
        mapped_addr = (nPRGBanks * 2 - 1) * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper_075::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{

    uint8_t total8K = nPRGBanks * 2;
    uint8_t total4K = (nCHRBanks == 0) ? 1 : nCHRBanks * 2;
    if (addr >= 0x8000 && addr <= 0xFFFF)
    {
        uint16_t reg = addr & 0xF000;
        if (addr >= 0x8000 && addr <= 0x8FFF) {
            prg_bank[0] = (data & 0x0F) % total8K;
        }
        else if (addr >= 0x9000 && addr <= 0x9FFF)
        {
            mirroring_mode = data & 0x01;

            chr_bit_high_0 = (data >> 1) & 0x01;
            chr_bit_high_1 = (data >> 2) & 0x01;

            chr_bank[0] =
                ((chr_bank[0] & 0x0F) | (chr_bit_high_0 << 4)) % total4K;

            chr_bank[1] =
                ((chr_bank[1] & 0x0F) | (chr_bit_high_1 << 4)) % total4K;
        }
        else if (addr >= 0xA000 && addr <= 0xAFFF) {
            prg_bank[1] = (data & 0x0F) % total8K;
        }
        else if (addr >= 0xC000 && addr <= 0xCFFF) {
            prg_bank[2] = (data & 0x0F) % total8K;
        }
        else if (addr >= 0xE000 && addr <= 0xEFFF) {
            chr_bank[0] =
                (((data & 0x0F) | (chr_bit_high_0 << 4)) % total4K);
        }
        else if (addr >= 0xF000) {
            chr_bank[1] =
                (((data & 0x0F) | (chr_bit_high_1 << 4)) % total4K);
        }
        return true;
    }
    return false;
}

bool Mapper_075::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x0FFF)
    {
        mapped_addr = chr_bank[0] * 0x1000 + (addr & 0x0FFF);
        return true;
    }

    if (addr <= 0x1FFF)
    {
        mapped_addr = chr_bank[1] * 0x1000 + (addr & 0x0FFF);
        return true;
    }

    return false;
}
bool Mapper_075::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF && nCHRBanks == 0)
    {
        mapped_addr = addr;
        return true;
    }

    return false;
}
MIRROR Mapper_075::mirror()
{
    return (mirroring_mode == 1) ? MIRROR::HORIZONTAL : MIRROR::VERTICAL;
}

std::string Mapper_075::GetDebugInfo()
{
    std::string s;
    s += "===== DEBUG VRC1 (MAPPER 075) =====\n\n";
    s += "PRG Bank 0: " + std::to_string(prg_bank[0]) + "\n";
    s += "PRG Bank 1: " + std::to_string(prg_bank[1]) + "\n";
    s += "PRG Bank 2: " + std::to_string(prg_bank[2]) + "\n";
    s += "CHR Bank 0: " + std::to_string(chr_bank[0]) + "\n";
    s += "CHR Bank 1: " + std::to_string(chr_bank[1]) + "\n";
    return s;
}