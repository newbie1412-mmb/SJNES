#include "PPU.h"
#include <qDebug>
#include "Mapper_004.h"
#include "Mapper_005.h"
#include "Mapper_024.h"
#include "Mapper_476.h"
#include "BinaryIO.h"
// KHỞI TẠO PPU & BẢNG MÀU
PPU::PPU() {
    palScreen[0x00] = { (uint8_t)102, (uint8_t)102, (uint8_t)102 };
    palScreen[0x01] = { (uint8_t)0, (uint8_t)42, (uint8_t)136 };
    palScreen[0x02] = { (uint8_t)20, (uint8_t)18, (uint8_t)167 };
    palScreen[0x03] = { (uint8_t)59, (uint8_t)0, (uint8_t)164 };
    palScreen[0x04] = { (uint8_t)92, (uint8_t)0, (uint8_t)126 };
    palScreen[0x05] = { (uint8_t)110, (uint8_t)0, (uint8_t)64 };
    palScreen[0x06] = { (uint8_t)108, (uint8_t)6, (uint8_t)0 };
    palScreen[0x07] = { (uint8_t)86, (uint8_t)29, (uint8_t)0 };
    palScreen[0x08] = { (uint8_t)51, (uint8_t)53, (uint8_t)0 };
    palScreen[0x09] = { (uint8_t)11, (uint8_t)72, (uint8_t)0 };
    palScreen[0x0A] = { (uint8_t)0, (uint8_t)82, (uint8_t)0 };
    palScreen[0x0B] = { (uint8_t)0, (uint8_t)79, (uint8_t)8 };
    palScreen[0x0C] = { (uint8_t)0, (uint8_t)64, (uint8_t)77 };
    palScreen[0x0D] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x0E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x0F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x10] = { (uint8_t)173, (uint8_t)173, (uint8_t)173 };
    palScreen[0x11] = { (uint8_t)21, (uint8_t)95, (uint8_t)217 };
    palScreen[0x12] = { (uint8_t)66, (uint8_t)64, (uint8_t)255 };
    palScreen[0x13] = { (uint8_t)117, (uint8_t)39, (uint8_t)254 };
    palScreen[0x14] = { (uint8_t)160, (uint8_t)26, (uint8_t)204 };
    palScreen[0x15] = { (uint8_t)183, (uint8_t)30, (uint8_t)123 };
    palScreen[0x16] = { (uint8_t)181, (uint8_t)49, (uint8_t)32 };
    palScreen[0x17] = { (uint8_t)153, (uint8_t)78, (uint8_t)0 };
    palScreen[0x18] = { (uint8_t)107, (uint8_t)109, (uint8_t)0 };
    palScreen[0x19] = { (uint8_t)56, (uint8_t)135, (uint8_t)0 };
    palScreen[0x1A] = { (uint8_t)12, (uint8_t)147, (uint8_t)0 };
    palScreen[0x1B] = { (uint8_t)0, (uint8_t)143, (uint8_t)50 };
    palScreen[0x1C] = { (uint8_t)0, (uint8_t)124, (uint8_t)141 };
    palScreen[0x1D] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x1E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x1F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x20] = { (uint8_t)255, (uint8_t)254, (uint8_t)255 };
    palScreen[0x21] = { (uint8_t)100, (uint8_t)176, (uint8_t)255 };
    palScreen[0x22] = { (uint8_t)146, (uint8_t)144, (uint8_t)255 };
    palScreen[0x23] = { (uint8_t)198, (uint8_t)118, (uint8_t)255 };
    palScreen[0x24] = { (uint8_t)243, (uint8_t)106, (uint8_t)255 };
    palScreen[0x25] = { (uint8_t)254, (uint8_t)110, (uint8_t)204 };
    palScreen[0x26] = { (uint8_t)254, (uint8_t)129, (uint8_t)112 };
    palScreen[0x27] = { (uint8_t)234, (uint8_t)158, (uint8_t)34 };
    palScreen[0x28] = { (uint8_t)188, (uint8_t)190, (uint8_t)0 };
    palScreen[0x29] = { (uint8_t)136, (uint8_t)216, (uint8_t)0 };
    palScreen[0x2A] = { (uint8_t)92, (uint8_t)228, (uint8_t)48 };
    palScreen[0x2B] = { (uint8_t)69, (uint8_t)224, (uint8_t)130 };
    palScreen[0x2C] = { (uint8_t)72, (uint8_t)205, (uint8_t)222 };
    palScreen[0x2D] = { (uint8_t)79, (uint8_t)79, (uint8_t)79 };
    palScreen[0x2E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x2F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };

    palScreen[0x30] = { (uint8_t)255, (uint8_t)254, (uint8_t)255 };
    palScreen[0x31] = { (uint8_t)192, (uint8_t)223, (uint8_t)255 };
    palScreen[0x32] = { (uint8_t)211, (uint8_t)210, (uint8_t)255 };
    palScreen[0x33] = { (uint8_t)232, (uint8_t)200, (uint8_t)255 };
    palScreen[0x34] = { (uint8_t)251, (uint8_t)194, (uint8_t)255 };
    palScreen[0x35] = { (uint8_t)254, (uint8_t)196, (uint8_t)234 };
    palScreen[0x36] = { (uint8_t)254, (uint8_t)204, (uint8_t)197 };
    palScreen[0x37] = { (uint8_t)247, (uint8_t)216, (uint8_t)165 };
    palScreen[0x38] = { (uint8_t)228, (uint8_t)229, (uint8_t)148 };
    palScreen[0x39] = { (uint8_t)207, (uint8_t)239, (uint8_t)150 };
    palScreen[0x3A] = { (uint8_t)189, (uint8_t)244, (uint8_t)171 };
    palScreen[0x3B] = { (uint8_t)179, (uint8_t)243, (uint8_t)204 };
    palScreen[0x3C] = { (uint8_t)181, (uint8_t)235, (uint8_t)242 };
    palScreen[0x3D] = { (uint8_t)184, (uint8_t)184, (uint8_t)184 };
    palScreen[0x3E] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
    palScreen[0x3F] = { (uint8_t)0, (uint8_t)0, (uint8_t)0 };
}

PPU::~PPU() {}

void PPU::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void PPU::reset() {
    fine_x = 0x00;
    address_latch = 0x00;
    ppu_data_buffer = 0x00;

    scanline = 0;
    cycle = 0;

    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;

    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;

    status = 0x00;
    ppu_mask = 0x00;
    ppu_ctrl = 0x00;

    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;

    nmi_requested = false;
    oam_addr = 0x00;

    mapper_a12 = false;
    mapper_a12_low_cycles = 255;

    sprite_count = 0;

    for (int i = 0; i < 256; i++)
        OAM[i] = 0x00;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            tblName[t][i] = 0x00;

    for (int i = 0; i < 32; i++)
        tblPalette[i] = 0x00;

    for (int i = 0; i < 64; i++)
    {
        sprite_pattern_lo[i] = 0x00;
        sprite_pattern_hi[i] = 0x00;
        sprite_x[i] = 0x00;
        sprite_attribute[i] = 0x00;
        sprite_zero_being_rendered[i] = false;
    }

    for (int i = 0; i < 256 * 240; i++) {
        RGBColor c = palScreen[0x0F];
        frame_pixels[i] = 0xFF000000 | (c.r << 16) | (c.g << 8) | c.b;
    }
}

// GIAO TIẾP VỚI CPU
uint8_t PPU::cpuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    if (rdonly) {
        switch (addr) {
        case 0x0000: data = ppu_ctrl; break;
        case 0x0001: data = ppu_mask; break;
        case 0x0002: data = status; break;
        case 0x0003: data = 0; break;
        case 0x0004: data = 0; break;
        case 0x0005: data = 0; break;
        case 0x0006: data = 0; break;
        case 0x0007: data = 0; break;
        }
    }
    else {
        switch (addr) {
        case 0x0000: break;
        case 0x0001: break;
        case 0x0002:
            data = (status & 0xE0) | (ppu_data_buffer & 0x1F);
            status &= ~0x80; address_latch = 0;
            break;
        case 0x0003: break;
        case 0x0004: data = OAM[oam_addr]; break;
        case 0x0005: break;
        case 0x0006: break;
        case 0x0007:
        {
            uint16_t addr = vram_addr.reg & 0x3FFF;
            NotifyMapperA12(addr);
            if (addr >= 0x3F00)
            {
                data = ppuRead(addr);
                ppu_data_buffer = ppuRead(addr - 0x1000);
            }
            else
            {
                data = ppu_data_buffer;
                ppu_data_buffer = ppuRead(addr);
            }

            vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
        }
        break;
        }
    }
    return data;
}

void PPU::cpuWrite(uint16_t addr, uint8_t data) {
    static int scrollLogCount = 0;

    auto logScrollWrite = [&](const char* regName, uint8_t value)
        {
            if (scrollLogCount < 300)
            {
                scrollLogCount++;
            }
        };
    switch (addr) {
    case 0x0000:
    {
        bool nmi_was_enabled = (ppu_ctrl & 0x80) > 0;

        ppu_ctrl = data;
        tram_addr.nametable_x = ppu_ctrl & 0x01;
        tram_addr.nametable_y = (ppu_ctrl & 0x02) >> 1;
        if (!nmi_was_enabled && (ppu_ctrl & 0x80) && (status & 0x80)) {
            nmi_requested = true;
        }
    }
    break;
    case 0x0001:
    {
        static uint8_t lastMask = 0xFF;
        if (data != lastMask)
        {
            lastMask = data;
            // (đã tắt log PPUMASK CHANGE, không cần nữa)
        }
    }
    ppu_mask = data;
    break;
    case 0x0002: break;
    case 0x0003: oam_addr = data; break;
    case 0x0004: OAM[oam_addr] = data; oam_addr++; break;
    case 0x0005: // PPUSCROLL
    {
        if (address_latch == 0)
        {
            fine_x = data & 0x07;
            tram_addr.coarse_x = data >> 3;
            address_latch = 1;
        }
        else
        {
            tram_addr.fine_y = data & 0x07;
            tram_addr.coarse_y = data >> 3;
            address_latch = 0;
        }
    }
    break;
    case 0x0006:
    {
        if (address_latch == 0) {
            tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
            address_latch = 1;
        }
        else {
            tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
            vram_addr = tram_addr;
            address_latch = 0;
            NotifyMapperA12(vram_addr.reg);
        }
    }
    break;
    case 0x0007:
    {
        NotifyMapperA12(vram_addr.reg & 0x3FFF);

        ppuWrite(vram_addr.reg, data);
        vram_addr.reg += (ppu_ctrl & 0x04) ? 32 : 1;
    }
    break;
    }
}

//BỘ NHỚ BUS GIAO TIẾP VỚI CARD ĐỒ HỌA
uint8_t PPU::ppuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;
    if (addr <= 0x1FFF)
        NotifyMapperA12(addr);
    if (cart && cart->ppuRead(addr, data)) return data;
    else if (addr <= 0x1FFF)
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    data = tblName[0][offset];
                    return data;

                case 1:
                    data = tblName[1][offset];
                    return data;

                case 2:
                    data = mmc5->ReadExRam(offset);
                    return data;

                case 3:
                    if (offset < 0x03C0)
                        data = mmc5->GetFillTile();
                    else
                        data = mmc5->GetFillAttr();

                    return data;
                }
            }
        }
        // VRC6 nametable banking
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                data = tblName[source & 0x01][offset];
                return data;
            }
        }
        if (cart && cart->pMapper)
        {
            if (auto* m476 = dynamic_cast<Mapper_476*>(cart->pMapper.get()))
            {
                static int logCount = 0;
                if (logCount < 5) {
                    qDebug() << "PPU dang doc nametable tu Mapper476, offset=" << offset;
                    logCount++;
                }
                data = m476->ReadFrameNametable(offset);
                return data;
            }
        }
        // Mirroring cũ cho mapper thường
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[1][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[0][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = tblName[0][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = tblName[1][addr & 0x03FF];
            else data = tblName[1][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            data = tblName[0][addr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            data = tblName[1][addr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        data = tblPalette[addr] & 0x3F;
    }

    return data;
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (addr <= 0x1FFF)
    {
        if (cart && cart->ppuWrite(addr, data))
            return;

        tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF] = data;
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;
        int ntIndex = ntAddr / 0x0400;
        uint16_t offset = ntAddr & 0x03FF;

        // MMC5 nametable mapping
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                uint8_t source = mmc5->GetNtSource(ntIndex);

                switch (source)
                {
                case 0:
                    tblName[0][offset] = data;
                    return;

                case 1:
                    tblName[1][offset] = data;
                    return;

                case 2:
                    // ExRAM làm nametable
                    mmc5->WriteExRam(offset, data);
                    return;

                case 3:
                    // Fill mode không ghi trực tiếp vào nametable
                    return;
                }
            }
        }
        // VRC6 nametable banking
        if (cart && cart->pMapper)
        {
            if (auto* vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get()))
            {
                uint8_t source = vrc6->GetNtSource(ntIndex);
                tblName[source & 0x01][offset] = data;
                return;
            }
        }
        // Mirroring cũ cho mapper thường
        addr = ntAddr;
        MIRROR m = cart->mirror;
        if (cart->pMapper != nullptr) {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE) {
                m = mapper_mirror;
            }
        }
        if (m == MIRROR::VERTICAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[1][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[0][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::HORIZONTAL) {
            if (addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) tblName[0][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) tblName[1][addr & 0x03FF] = data;
            else tblName[1][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_LO) {
            tblName[0][addr & 0x03FF] = data;
        }
        else if (m == MIRROR::ONESCREEN_HI) {
            tblName[1][addr & 0x03FF] = data;
        }

    }
    else if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000; if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008; if (addr == 0x001C) addr = 0x000C;
        tblPalette[addr] = data & 0x3F;
    }
}


void PPU::LoadBackgroundShifters() {
    bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
    bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
    bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
    bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
}

void PPU::UpdateShifters() {
    if (ppu_mask & 0x18) {
        bg_shifter_pattern_lo <<= 1; bg_shifter_pattern_hi <<= 1;
        bg_shifter_attrib_lo <<= 1; bg_shifter_attrib_hi <<= 1;
    }
}

//PHẦN CHÍNH
void PPU::Step() {
    if (!mapper_a12 && mapper_a12_low_cycles < 255)
    {
        mapper_a12_low_cycles++;
    }
    if (cycle == 0)
    {
        if (cart && cart->pMapper)
        {
            if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
            {
                mmc5->NotifyScanline(scanline);
            }
        }
    }
    auto lam_IncScrollX = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.coarse_x == 31) { vram_addr.coarse_x = 0; vram_addr.nametable_x = ~vram_addr.nametable_x; }
            else { vram_addr.coarse_x++; }
        }
        };
    auto lam_IncScrollY = [&]() {
        if (ppu_mask & 0x18) {
            if (vram_addr.fine_y < 7) { vram_addr.fine_y++; }
            else {
                vram_addr.fine_y = 0;
                if (vram_addr.coarse_y == 29) { vram_addr.coarse_y = 0; vram_addr.nametable_y = ~vram_addr.nametable_y; }
                else if (vram_addr.coarse_y == 31) { vram_addr.coarse_y = 0; }
                else { vram_addr.coarse_y++; }
            }
        }
        };
    auto lam_TransAddrX = [&]() { if (ppu_mask & 0x18) { vram_addr.nametable_x = tram_addr.nametable_x; vram_addr.coarse_x = tram_addr.coarse_x; } };
    auto lam_TransAddrY = [&]() { if (ppu_mask & 0x18) { vram_addr.fine_y = tram_addr.fine_y; vram_addr.nametable_y = tram_addr.nametable_y; vram_addr.coarse_y = tram_addr.coarse_y; } };

    if (scanline >= -1 && scanline < 240) {
        if (scanline == -1 && cycle == 1) status &= ~0xE0;
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            UpdateShifters();

            if (ppu_mask & 0x18) {
                switch ((cycle - 1) % 8) {
                case 0:
                    LoadBackgroundShifters();
                    NotifyMapperA12(0x0000);
                    bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
                    break;
                case 2:
                {
                    NotifyMapperA12(0x0000);

                    bool usedMmc5ExtendedAttr = false;

                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                        {
                            if (mmc5->GetExRamMode() == 1)
                            {
                                uint16_t exOffset =
                                    (uint16_t(vram_addr.coarse_y) * 32)
                                    + vram_addr.coarse_x;

                                uint8_t ex = mmc5->ReadExRam(exOffset);

                                // bit 6-7: palette cho tile hiện tại
                                bg_next_tile_attrib = (ex >> 6) & 0x03;

                                // bit 0-5: CHR 4KB page cho tile hiện tại
                                mmc5->SetExtendedChrBank(ex & 0x3F);

                                usedMmc5ExtendedAttr = true;
                            }
                        }
                    }

                    if (!usedMmc5ExtendedAttr)
                    {
                        bg_next_tile_attrib = ppuRead(
                            0x23C0
                            | (vram_addr.nametable_y << 11)
                            | (vram_addr.nametable_x << 10)
                            | ((vram_addr.coarse_y >> 2) << 3)
                            | (vram_addr.coarse_x >> 2)
                        );

                        if (vram_addr.coarse_y & 0x02)
                            bg_next_tile_attrib >>= 4;

                        if (vram_addr.coarse_x & 0x02)
                            bg_next_tile_attrib >>= 2;

                        bg_next_tile_attrib &= 0x03;
                    }
                }
                break;
                case 4:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_lsb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 0
                    );
                }
                break;

                case 6:
                {
                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeBackground();
                    }

                    bg_next_tile_msb = ppuRead(
                        ((ppu_ctrl & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y
                        + 8
                    );
                }
                break;
                case 7: lam_IncScrollX(); break;
                }
            }
        }
        if (cycle == 256) lam_IncScrollY();
        if (cycle == 257) {
            LoadBackgroundShifters();
            lam_TransAddrX();

            if (ppu_mask & 0x18) {
                sprite_count = 0;

                int target_scanline = scanline + 1;

                // Không evaluate sprite ngoài vùng visible
                if (target_scanline >= 0 && target_scanline < 240) {
                    int spriteLimit = bRemoveSpriteLimit ? 64 : 8;
                    int foundSprites = 0;
                    for (int i = 0; i < 64; i++)
                    {
                        uint8_t sprite_y = OAM[i * 4 + 0];
                        uint8_t sprite_id = OAM[i * 4 + 1];
                        uint8_t sprite_attr = OAM[i * 4 + 2];
                        uint8_t sprite_x_pos = OAM[i * 4 + 3];

                        int spriteHeight = (ppu_ctrl & 0x20) ? 16 : 8;
                        int diffY = target_scanline - (sprite_y + 1);

                        if (diffY >= 0 && diffY < spriteHeight)
                        {
                            foundSprites++;

                            if (foundSprites > 8)
                                status |= 0x20; // sprite overflow, gần đúng

                            if (sprite_count >= spriteLimit)
                                continue;

                            bool flipY = (sprite_attr & 0x80) != 0;
                            int row = flipY ? (spriteHeight - 1 - diffY) : diffY;

                            uint16_t pattern_addr = 0;

                            if (spriteHeight == 8)
                            {
                                pattern_addr =
                                    ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) |
                                    ((uint16_t)sprite_id << 4) |
                                    row;
                            }
                            else
                            {
                                uint16_t table = (sprite_id & 0x01) ? 0x1000 : 0x0000;
                                uint16_t tile = (sprite_id & 0xFE);

                                if (row < 8)
                                    pattern_addr = table | (tile << 4) | row;
                                else
                                    pattern_addr = table | ((tile + 1) << 4) | (row - 8);
                            }

                            if (cart && cart->pMapper)
                            {
                                if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                                    mmc5->SetChrFetchModeSprite();
                            }

                            sprite_pattern_lo[sprite_count] = ppuRead(pattern_addr);
                            sprite_pattern_hi[sprite_count] = ppuRead(pattern_addr + 8);
                            sprite_x[sprite_count] = sprite_x_pos;
                            sprite_attribute[sprite_count] = sprite_attr;
                            sprite_zero_being_rendered[sprite_count] = (i == 0);

                            sprite_count++;
                        }
                    }
                }

                // dummy sprite fetch cho các slot còn lại
                for (int i = sprite_count; i < 8; i++) {
                    uint16_t dummy_addr = ((ppu_ctrl & 0x08) ? 0x1000 : 0x0000) | (0xFF << 4);

                    if (cart && cart->pMapper)
                    {
                        if (auto* mmc5 = dynamic_cast<Mapper_005*>(cart->pMapper.get()))
                            mmc5->SetChrFetchModeSprite();
                    }

                    ppuRead(dummy_addr);
                    ppuRead(dummy_addr + 8);
                }
            }
        }
        if (cycle == 338 || cycle == 340) {
            if (ppu_mask & 0x18) {
                NotifyMapperA12(0x0000);
                bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
            }
        }
        if (scanline == -1 && cycle >= 280 && cycle < 305) lam_TransAddrY();
    }

    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256) {
        uint8_t bg_pixel = 0x00; uint8_t bg_palette = 0x00;
        if (ppu_mask & 0x08) {
            uint16_t bit_mux = 0x8000 >> fine_x;
            uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0; uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
            bg_pixel = (p1_pixel << 1) | p0_pixel;
            uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0; uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
            bg_palette = (bg_pal1 << 1) | bg_pal0;
        }

        uint8_t fg_pixel = 0x00; uint8_t fg_palette = 0x00; uint8_t fg_priority = 0; bool bSpriteZeroBeingRendered = false;
        if (ppu_mask & 0x10) {
            for (int i = 0; i < sprite_count; i++) {
                int diffX = (cycle - 1) - sprite_x[i];
                if (diffX >= 0 && diffX < 8) {
                    uint8_t flipX = (sprite_attribute[i] & 0x40) > 0;
                    int col = flipX ? (7 - diffX) : diffX;
                    uint8_t bit_mux = 0x80 >> col;

                    if ((sprite_pattern_lo[i] & bit_mux) || (sprite_pattern_hi[i] & bit_mux)) {
                        uint8_t p0_pixel = (sprite_pattern_lo[i] & bit_mux) > 0;
                        uint8_t p1_pixel = (sprite_pattern_hi[i] & bit_mux) > 0;
                        fg_pixel = (p1_pixel << 1) | p0_pixel;
                        fg_palette = (sprite_attribute[i] & 0x03) + 0x04;
                        fg_priority = (sprite_attribute[i] & 0x20) == 0;
                        bSpriteZeroBeingRendered = sprite_zero_being_rendered[i];
                        break;
                    }
                }
            }
            if (!(ppu_mask & 0x02) && (cycle >= 1 && cycle <= 8)) bg_pixel = 0;
            if (!(ppu_mask & 0x04) && (cycle >= 1 && cycle <= 8)) fg_pixel = 0;
        }

        if (bSpriteZeroBeingRendered && (ppu_mask & 0x18) == 0x18) {
            if (cycle >= 1 && cycle < 256) {
                if (bg_pixel > 0 && fg_pixel > 0) {
                    status |= 0x40;
                }
            }
        }
        uint8_t final_pixel = 0;
        uint8_t final_palette = 0;
        if (bg_pixel == 0 && fg_pixel == 0) {
            final_pixel = 0; final_palette = 0;
        }
        else
            if (bg_pixel == 0 && fg_pixel > 0) {
                final_pixel = fg_pixel; final_palette = fg_palette;
            }
            else
                if (bg_pixel > 0 && fg_pixel == 0) {
                    final_pixel = bg_pixel; final_palette = bg_palette;
                }
                else {
                    if (fg_priority) {
                        final_pixel = fg_pixel;
                        final_palette = fg_palette;
                    }
                    else {
                        final_pixel = bg_pixel;
                        final_palette = bg_palette;
                    }
                }

        uint8_t pal_idx = (final_pixel != 0)
            ? (ppuRead(0x3F00 + (final_palette << 2) + final_pixel) & 0x3F)
            : (ppuRead(0x3F00) & 0x3F);

        if (ppu_mask & 0x01)
            pal_idx &= 0x30;

        {
            RGBColor c = palScreen[pal_idx];
            frame_pixels[scanline * 256 + (cycle - 1)] = 0xFF000000 | (c.r << 16) | (c.g << 8) | c.b;
        }
    }

    if (scanline == 241 && cycle == 1) {
        status |= 0x80;
        if (ppu_ctrl & 0x80) nmi_requested = true;
    }

    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 261) {
            scanline = -1;
        }
    }

}

const uint32_t* PPU::GetScreenBuffer() const {
    return frame_pixels;
}
void PPU::NotifyMapperA12(uint16_t addr)
{
    bool new_a12 = (addr & 0x1000) != 0;

    if (!mapper_a12 && new_a12)
    {
        if (mapper_a12_low_cycles >= 3)
        {
            if (cart && cart->pMapper)
            {
                cart->pMapper->ClockA12();
            }
        }

        mapper_a12_low_cycles = 0;
    }

    if (mapper_a12 && !new_a12)
    {
        mapper_a12_low_cycles = 0;
    }

    mapper_a12 = new_a12;
}
uint8_t PPU::GetOAMByte(uint8_t index) const
{
    return OAM[index];
}

uint8_t PPU::GetPPUCtrl() const
{
    return ppu_ctrl;
}

PPU::RGBColor PPU::GetNESColor(uint8_t index) const {
    return palScreen[index & 0x3F];
}
uint8_t PPU::DebugPPURead(uint16_t addr)
{
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (cart && cart->ppuRead(addr, data))
    {
        return data;
    }
    else if (addr <= 0x1FFF)
    {
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    }
    else if (addr >= 0x2000 && addr <= 0x3EFF)
    {
        uint16_t ntAddr = addr & 0x0FFF;

        MIRROR m = cart->mirror;

        if (cart->pMapper != nullptr)
        {
            MIRROR mapper_mirror = cart->pMapper->mirror();
            if (mapper_mirror != MIRROR::HARDWARE)
            {
                m = mapper_mirror;
            }
        }

        if (m == MIRROR::VERTICAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[1][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[0][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::HORIZONTAL)
        {
            if (ntAddr <= 0x03FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x07FF) data = tblName[0][ntAddr & 0x03FF];
            else if (ntAddr <= 0x0BFF) data = tblName[1][ntAddr & 0x03FF];
            else data = tblName[1][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_LO)
        {
            data = tblName[0][ntAddr & 0x03FF];
        }
        else if (m == MIRROR::ONESCREEN_HI)
        {
            data = tblName[1][ntAddr & 0x03FF];
        }
    }
    else if (addr >= 0x3F00)
    {
        addr &= 0x001F;

        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;

        data = tblPalette[addr] & 0x3F;
    }

    return data;
}
void PPU::SetExtraScanlinesBeforeNMI(int value)
{
    if (value < 0) value = 0;
    if (value > 300) value = 300; // giới hạn an toàn
    extraScanlinesBeforeNMI = value;
}

int PPU::GetExtraScanlinesBeforeNMI() const
{
    return extraScanlinesBeforeNMI;
}
void PPU::SaveState(BinaryWriter& out) const
{
    out << nmi_requested;
    out << oam_addr;

    for (int i = 0; i < 256; i++)
        out << OAM[i];

    out << mapper_a12;
    out << mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            out << tblName[t][i];

    for (int i = 0; i < 32; i++)
        out << tblPalette[i];

    out << ppu_ctrl;
    out << ppu_mask;
    out << status;
    out << ppu_data_buffer;
    out << address_latch;

    out << vram_addr.reg;
    out << tram_addr.reg;
    out << fine_x;

    out << scanline;
    out << cycle;

    out << bg_next_tile_id;
    out << bg_next_tile_attrib;
    out << bg_next_tile_lsb;
    out << bg_next_tile_msb;

    out << bg_shifter_pattern_lo;
    out << bg_shifter_pattern_hi;
    out << bg_shifter_attrib_lo;
    out << bg_shifter_attrib_hi;

    out << extraScanlinesBeforeNMI;
    out << bRemoveSpriteLimit;
    out << sprite_count;

    for (int i = 0; i < 64; i++)
    {
        out << sprite_pattern_lo[i];
        out << sprite_pattern_hi[i];
        out << sprite_x[i];
        out << sprite_attribute[i];
        out << sprite_zero_being_rendered[i];
    }
}

void PPU::LoadState(BinaryReader& in)
{
    in >> nmi_requested;
    in >> oam_addr;

    for (int i = 0; i < 256; i++)
        in >> OAM[i];

    in >> mapper_a12;
    in >> mapper_a12_low_cycles;

    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            in >> tblName[t][i];

    for (int i = 0; i < 32; i++)
        in >> tblPalette[i];

    in >> ppu_ctrl;
    in >> ppu_mask;
    in >> status;
    in >> ppu_data_buffer;
    in >> address_latch;

    in >> vram_addr.reg;
    in >> tram_addr.reg;
    in >> fine_x;

    in >> scanline;
    in >> cycle;

    in >> bg_next_tile_id;
    in >> bg_next_tile_attrib;
    in >> bg_next_tile_lsb;
    in >> bg_next_tile_msb;

    in >> bg_shifter_pattern_lo;
    in >> bg_shifter_pattern_hi;
    in >> bg_shifter_attrib_lo;
    in >> bg_shifter_attrib_hi;

    in >> extraScanlinesBeforeNMI;
    in >> bRemoveSpriteLimit;
    in >> sprite_count;

    for (int i = 0; i < 64; i++)
    {
        in >> sprite_pattern_lo[i];
        in >> sprite_pattern_hi[i];
        in >> sprite_x[i];
        in >> sprite_attribute[i];
        in >> sprite_zero_being_rendered[i];
    }
}