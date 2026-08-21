#pragma once
#include <cstdint>
#include <memory>
#include "Cartridge.h"
#include "BinaryIO.h"
class PPU {
public:
    PPU();
    ~PPU();
    void SetRemoveSpriteLimit(bool enable) { bRemoveSpriteLimit = enable; }
    //ép xung =))
    void SetExtraScanlinesBeforeNMI(int value);
    int GetExtraScanlinesBeforeNMI() const;
    void ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge);
    void reset();
    void Step(); // Chạy 1 chu kỳ PPU
    uint8_t getPPUMask() { return ppu_mask; }
    uint8_t getPPUCtrl() { return ppu_ctrl; }
    struct RGBColor { uint8_t r, g, b; };
    RGBColor palScreen[0x40];
    RGBColor GetNESColor(uint8_t index) const;
    // GIAO TIẾP VỚI CPU (Đọc/Ghi các thanh ghi $2000 - $2007)
    uint8_t cpuRead(uint16_t addr, bool rdonly = false);
    void cpuWrite(uint16_t addr, uint8_t data);

    // GIAO TIẾP VỚI BĂNG GAME VÀ BỘ NHỚ PPU ($0000 - $3FFF)
    uint8_t ppuRead(uint16_t addr, bool rdonly = false);
    void ppuWrite(uint16_t addr, uint8_t data);

    // XUẤT HÌNH ẢNH RA QT CỦA ANH
    const uint32_t* GetScreenBuffer() const;

    // HÀM PHỤ TRỢ CHO DEBUG / SPRITE VIEWER
    uint8_t DebugPPURead(uint16_t addr);
    uint8_t GetOAMByte(uint8_t index) const;
    uint8_t GetPPUCtrl() const;
    

    // CÁC BIẾN TRẠNG THÁI QUAN TRỌNG ĐỂ BUS.CPP VÀ UI GỌI
    bool nmi_requested = false;
    uint8_t oam_addr = 0x00; 
    uint8_t OAM[256];
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);

    uint8_t GetPaletteRAM(uint8_t index) const { return tblPalette[index & 0x1F]; }
private:
    bool bRemoveSpriteLimit = false;
    int sprite_count = 0;
    uint8_t sprite_pattern_lo[64];
    uint8_t sprite_pattern_hi[64];
    uint8_t sprite_x[64];
    uint8_t sprite_attribute[64];
    bool sprite_zero_being_rendered[64];
    std::shared_ptr<Cartridge> cart;
    bool mapper_a12 = false;
    uint8_t mapper_a12_low_cycles = 0;
    void NotifyMapperA12(uint16_t addr);
    // BỘ NHỚ NỘI BỘ CỦA PPU
    uint8_t tblName[2][1024];    // 2 Nametables (2KB)
    uint8_t tblPattern[2][4096]; // 2 Pattern Tables (8KB - Thường do Cartridge đè lên)
    uint8_t tblPalette[32];      // Bảng màu Palette (32 bytes)

    // Khung hình để vẽ lên (256x240 pixels)
    uint32_t frame_pixels[256 * 240];

    // CÁC THANH GHI ĐIỀU KHIỂN CHÍNH
    uint8_t ppu_ctrl = 0x00;   // $2000
    uint8_t ppu_mask = 0x00;   // $2001
    uint8_t status = 0x00;     // $2002
    uint8_t ppu_data_buffer = 0x00; // Bộ đệm cho lệnh đọc $2007 trễ 1 nhịp
    uint8_t address_latch = 0x00;   // Latch cho $2005 và $2006 (w)

    // THANH GHI CUỘN MÀN HÌNH "LOOPY REGISTER" TRỨ DANH (Siêu quan trọng)
    union loopy_register {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    };

    loopy_register vram_addr; // Địa chỉ PPU hiện tại (v)
    loopy_register tram_addr; // Địa chỉ tạm thời lưu tọa độ (t)
    uint8_t fine_x = 0x00;    // Cuộn ngang pixel lẻ (x)

    int16_t scanline = 0; // Dòng đang quét (0-261)
    int16_t cycle = 0;    // Chu kỳ hiện tại trên dòng (0-340)

    // Bộ nhớ tạm cho Background của Tile tiếp theo
    uint8_t bg_next_tile_id = 0x00;
    uint8_t bg_next_tile_attrib = 0x00;
    uint8_t bg_next_tile_lsb = 0x00;
    uint8_t bg_next_tile_msb = 0x00;

    // Thanh ghi trượt (Shifters) chứa dữ liệu 16 pixel để vẽ dần ra màn hình
    uint16_t bg_shifter_pattern_lo = 0x0000;
    uint16_t bg_shifter_pattern_hi = 0x0000;
    uint16_t bg_shifter_attrib_lo = 0x0000;
    uint16_t bg_shifter_attrib_hi = 0x0000;
    // Hàm phụ trợ cho việc quét Background
    void LoadBackgroundShifters();
    void UpdateShifters();
    //ép xung 
    int extraScanlinesBeforeNMI = 0;
};