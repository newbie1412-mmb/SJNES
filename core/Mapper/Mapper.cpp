#include "Mapper.h"

Mapper::Mapper(uint32_t prgBanks, uint32_t chrBanks) {
    nPRGBanks = prgBanks;
    nCHRBanks = chrBanks;
}

Mapper::~Mapper() {}