#include "Mapper_476.h"

Mapper_476::Mapper_476(uint32_t prgBanks, uint32_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

Mapper_476::~Mapper_476() {}

void Mapper_476::reset() {
    nPRGBankSelect = 0;
    nCHRBankSelect = 0;
}

bool Mapper_476::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mapped_addr = nPRGBankSelect * 0x8000 + (addr & 0x7FFF);
        return true;
    }
    return false;
}

bool Mapper_476::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    if (addr == 0x8000) {
        nCHRBankSelect = (nCHRBankSelect & 0xFF00) | data;
    }
    else if (addr == 0x8001) {
        nCHRBankSelect = (nCHRBankSelect & 0x00FF) | (data << 8);
    }
    else if (addr == 0x8002) {
        nPRGBankSelect = data;
        if (nPRGBanks > 0) nPRGBankSelect %= nPRGBanks;
    }
    return false;
}

bool Mapper_476::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr <= 0x1FFF) {
        mapped_addr = (uint32_t)nCHRBankSelect * 4096 + addr;
        return true;
    }
    return false;
}

bool Mapper_476::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    return false;
}

std::string Mapper_476::GetDebugInfo() {
    std::string s;
    s += "===== MAPPER 476 - CUSTOM (Bad Apple) =====\n\n";
    s += "PRG bank hien tai: " + std::to_string(nPRGBankSelect) + "\n";
    s += "CHR bank (frame) hien tai: " + std::to_string(nCHRBankSelect) + "\n";
    s += "So byte nametable da nap: " + std::to_string(vNametableData.size()) + "\n";
    return s;
}