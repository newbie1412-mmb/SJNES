#include "Mapper_026.h"

Mapper_026::Mapper_026(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper_024(prgBanks, chrBanks)
{
}

bool Mapper_026::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    // VRC6b: A0 và A1 bị đảo vai trò so với VRC6a khi đấu vào chip.
    // Cách xử lý: hoán đổi 2 bit thấp của địa chỉ trước khi đưa vào
    // logic gốc của VRC6a (Mapper_024), tái sử dụng toàn bộ audio/banking.
    uint16_t bit0 = addr & 0x0001;
    uint16_t bit1 = addr & 0x0002;

    uint16_t swappedLow2 = (bit0 << 1) | (bit1 >> 1);
    uint16_t swappedAddr = (addr & 0xFFFC) | swappedLow2;

    return Mapper_024::cpuMapWrite(swappedAddr, mapped_addr, data);
}