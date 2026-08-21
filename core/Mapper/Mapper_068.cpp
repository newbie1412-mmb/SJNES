#include "Mapper_068.h"

Mapper_068::Mapper_068(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

void Mapper_068::reset()
{
    memset(chrRegs, 0, sizeof(chrRegs));
    memset(ntRegs, 0, sizeof(ntRegs));

    useChrForNametables = false;

    mirrorMode = 0;

    prgBank = 0;

    prgRamEnabled = false;

    usingExternalRom = false;

    externalPage = 0;

    licensingTimer = 0;
}

//================================================
// CPU READ
//================================================
bool Mapper_068::cpuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        if (prgRamEnabled)
            licensingTimer = LICENSE_CYCLES;

        return false;
    }

    if (addr >= 0x8000 && addr <= 0xBFFF)
    {
        if (usingExternalRom && licensingTimer == 0)
        {
            mapped_addr = 0;
            return true;
        }

        uint8_t bank =
            usingExternalRom
            ? externalPage
            : prgBank;

        mapped_addr =
            bank * 0x4000
            + (addr & 0x3FFF);

        return true;
    }

    if (addr >= 0xC000)
    {
        mapped_addr =
            (nPRGBanks - 1) * 0x4000
            + (addr & 0x3FFF);

        return true;
    }

    return false;
}

//================================================
// CPU WRITE
//================================================
bool Mapper_068::cpuMapWrite(
    uint16_t addr,
    uint32_t& mapped_addr,
    uint8_t data)
{
    if (addr >= 0x6000 && addr <= 0x7FFF)
    {
        licensingTimer = LICENSE_CYCLES;
        return false;
    }

    if (addr < 0x8000)
        return false;
    switch (addr & 0xF000)
    {
    case 0x8000:

        chrRegs[0] = data;

        break;

    case 0x9000:

        chrRegs[1] = data;

        break;

    case 0xA000:

        chrRegs[2] = data;

        break;

    case 0xB000:

        chrRegs[3] = data;

        break;

    case 0xC000:
      
        ntRegs[0] = data | 0x80;     
        break;

    case 0xD000:
        ntRegs[1] = data | 0x80;
        break;

    case 0xE000:

        mirrorMode =
            data & 3;

        useChrForNametables =
            (data & 0x10);
        break;

    case 0xF000:
    {
        bool external =
            !(data & 0x08);

        if (
            external
            &&
            nPRGBanks > 8
            )
        {
            usingExternalRom = true;

            externalPage =
                8
                +
                (data & 7);
        }
        else
        {
            usingExternalRom = false;

            prgBank =
                data & 7;
        }

        prgRamEnabled =
            data & 0x10;

        break;
    }
    }

    return false;
}

// PPU READ
bool Mapper_068::ppuMapRead(
    uint16_t addr,
    uint32_t& mapped_addr)
{
    if (addr <= 0x1FFF)
    {
        uint8_t bank = addr >> 11;
        uint32_t currentBank = chrRegs[bank];
        if (nCHRBanks > 0)
        {
            currentBank %= (nCHRBanks * 4);
        }

        mapped_addr = currentBank * 0x800 + (addr & 0x7FF);
        return true;
    }

    if (addr >= 0x2000 && addr <= 0x2FFF)
    {
        mapped_addr = ResolveNametable(addr);

        static int c = 0;

        if (mapped_addr & 0x80000000)
            return false;

        return true;
    }

    return false;
}

// PPU WRITE
bool Mapper_068::ppuMapWrite(
    uint16_t addr,
    uint32_t& mapped_addr)
{
    if (
        addr <= 0x1FFF
        &&
        nCHRBanks == 0
        )
    {
        mapped_addr =
            chrRegs[addr >> 11]
            *
            0x800
            +
            (addr & 0x7FF);

        return true;
    }

    if (
        addr >= 0x2000
        &&
        addr <= 0x2FFF
        )
    {
        mapped_addr =
            ResolveNametable(addr);

        return
            !(mapped_addr
                &
                0x80000000);
    }

    return false;
}

// NAMETABLE
uint32_t Mapper_068::ResolveNametable(uint16_t addr)
{
    uint8_t slot = (addr >> 10) & 3;

    // CHR Nametable mode
    if (useChrForNametables)
    {
        uint8_t reg = 0;

        switch (mirrorMode)
        {
            // NOTE: swap mapping for Afterburner compatibility
        case 0: // Vertical
            reg = slot & 1;
            break;

        case 1: // Horizontal
            reg = slot >> 1;
            break;

        case 2: // One-screen low
            reg = 0;
            break;

        default: // One-screen high
            reg = 1;
            break;
        }

        uint8_t chrBank = ntRegs[reg];

        if (nCHRBanks > 0)
        {
            chrBank %= (nCHRBanks * 8);
        }

        return (uint32_t)chrBank * 0x400 + (addr & 0x3FF);
    }

    // VRAM mode
    uint8_t phys;

    switch (mirrorMode)
    {
    case 0:  phys = slot & 1;      break; // Vertical
    case 1:  phys = slot >> 1;     break; // Horizontal
    case 2:  phys = 0;             break; // One-screen low
    default: phys = 1;            break; // One-screen high
    }

    return 0x80000000
        | (phys << 10)
        | (addr & 0x3FF);
}