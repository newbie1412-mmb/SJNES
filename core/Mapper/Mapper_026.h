#pragma once
#include "Mapper_024.h"

// VRC6b (Konami chip 351949) — giống hệt VRC6a (Mapper_024) về mặt logic,
// chỉ khác cách đấu 2 chân địa chỉ A0/A1 trên board mạch.
class Mapper_026 : public Mapper_024
{
public:
    Mapper_026(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
};