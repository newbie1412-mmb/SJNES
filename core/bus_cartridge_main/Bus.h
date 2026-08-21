#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include "PPU.h"
#include "APU.h"
#include "Cartridge.h"
#include "CPU6502.h"
#include <functional>
class Bus {
public:
    Bus() {
        n_apu.bus = this;
        for (int i = 0; i < 2048; i++) {
            ram[i] = 0xFF;
        }
    }

    bool nsfMode = false;
    std::array<uint8_t, 65536> nsfMemory{};
    bool nsfBankMode = false;
    std::array<uint8_t, 8> nsfBanks{};
    std::vector<uint8_t> nsfBankData;
    std::function<bool(uint16_t, uint8_t)> nsfExpansionWrite;
    std::function<bool(uint16_t, uint8_t&)> nsfExpansionRead;

    void EnableNSFMode(
        uint16_t loadAddress,
        const std::vector<uint8_t>& data,
        const uint8_t* initialBanks = nullptr

    )
    {
        nsfMode = true;
        nsfExpansionWrite = nullptr;
        nsfMemory.fill(0x00);

        nsfBankMode = false;
        nsfBanks.fill(0x00);
        nsfBankData.clear();

        bool useBankSwitching = false;

        if (initialBanks)
        {
            for (int i = 0; i < 8; i++)
            {
                if (initialBanks[i] != 0)
                {
                    useBankSwitching = true;
                    break;
                }
            }
        }

        if (useBankSwitching)
        {
            nsfBankMode = true;

            int padding = loadAddress & 0x0FFF;

            nsfBankData.assign(padding, 0x00);
            nsfBankData.insert(nsfBankData.end(), data.begin(), data.end());

            for (int i = 0; i < 8; i++)
                nsfBanks[i] = initialBanks[i];
        }

        for (size_t i = 0; i < data.size(); i++)
        {
            uint32_t addr = static_cast<uint32_t>(loadAddress) + static_cast<uint32_t>(i);

            if (addr <= 0xFFFF)
                nsfMemory[addr] = data[i];
            else
                break;
        }
    }

    void DisableNSFMode()
    {
        nsfMode = false;
        nsfMemory.fill(0x00);

        nsfBankMode = false;
        nsfBanks.fill(0x00);
        nsfBankData.clear();
        nsfExpansionWrite = nullptr;
        nsfExpansionRead = nullptr;
    }

    uint8_t controller_state = 0x00;
    uint8_t controller_strobe = 0x00;
    uint8_t controller_shift = 0x00;
    uint8_t controller_state2 = 0x00;
    uint8_t controller_shift2 = 0x00;
    bool zapperConnected = false;
    bool zapperTrigger = false;
    int zapperX = -1;
    int zapperY = -1;


    // === CÁC BIẾN QUẢN LÝ DMA CHUẨN XÁC ===
    uint8_t dma_page = 0x00;
    uint8_t dma_addr = 0x00;
    uint8_t dma_data = 0x00;
    bool dma_dummy = true;
    bool dma_transfer = false;

    uint32_t system_clock_counter = 0; // Bộ đếm nhịp hệ thống

    // Giá trị "open bus" - byte cuối cùng thực sự nằm trên bus dữ liệu CPU.
    // Dùng để giả lập hành vi phần cứng thật khi đọc các port ghi-only
    // hoặc vùng địa chỉ chưa mapped ($4000-$4013, $4014, $4018-$40FF, ...).
    uint8_t open_bus = 0x00;

    PPU* ppu = nullptr;
    CPU6502* cpu = nullptr; // Con trỏ CPU
    APU n_apu;

    // Bộ nhớ RAM nội bộ (2KB)
    uint8_t ram[2048];

    // === KHE CẮM BĂNG GAME ===
    std::shared_ptr<Cartridge> cart;
    void insertCartridge(const std::shared_ptr<Cartridge>& cartridge) {
        this->cart = cartridge;
        if (ppu != nullptr) {
            ppu->ConnectCartridge(cartridge);
        }
    }

    void cpuWrite(uint16_t addr, uint8_t data) {
        // Mọi byte CPU ghi ra đều thực sự đi qua bus dữ liệu -> cập nhật open bus.
        open_bus = data;

        if (nsfMode && nsfBankMode && addr >= 0x5FF8 && addr <= 0x5FFF)
        {
            nsfBanks[addr - 0x5FF8] = data;
            return;
        }

        // Expansion audio NSF: VRC6 / VRC7 / S5B / MMC5
        if (nsfMode && nsfExpansionWrite)
        {
            if (nsfExpansionWrite(addr, data))
                return;
        }

        if (nsfMode && addr >= 0x4020)
        {
            if (!nsfBankMode || addr < 0x8000)
                nsfMemory[addr] = data;

            return;
        }


        if (cart != nullptr && cart->cpuWrite(addr, data)) {
            return;
        }

        if (addr >= 0x0000 && addr <= 0x1FFF) {
            ram[addr & 0x07FF] = data;
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu != nullptr)
                ppu->cpuWrite(addr & 0x0007, data);
        }
        // =========================================================
        // 2. SỬA DMA CÓ THỜI GIAN THẬT (REAL TIME OAM DMA)
        // =========================================================
        else if (addr == 0x4014) {
            dma_page = data;
            dma_addr = 0x00;
            dma_dummy = true;
            dma_transfer = true;
            for (int i = 0; i < 256; i++) {
                if (ppu != nullptr)
                    ppu->OAM[i] = cpuRead(((uint16_t)dma_page << 8) | i);
            }
        }
        else if (addr == 0x4016) {
            controller_strobe = data & 0x01;

            if (controller_strobe) {
                controller_shift = controller_state;    // tay 1
                controller_shift2 = controller_state2;  // tay 2
            }
        }
        else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 || addr == 0x4017) {
            n_apu.cpuWrite(addr, data);
        }
    }

    uint8_t cpuRead(uint16_t addr) {
        // Mặc định = giá trị open bus hiện tại. Nếu không nhánh nào bên dưới
        // khớp với addr (vùng chưa mapped hoặc port chỉ-ghi như $4000-$4013,
        // $4014, $4018-$40FF), hàm sẽ tự nhiên trả về đúng giá trị open bus
        // thay vì 0x00 cố định - đúng hành vi phần cứng NES thật.
        uint8_t data = open_bus;

        if (nsfMode && nsfBankMode && addr >= 0x8000)
        {
            int slot = (addr - 0x8000) >> 12;
            int bank = nsfBanks[slot];

            uint32_t offset =
                static_cast<uint32_t>(bank) * 0x1000 +
                static_cast<uint32_t>(addr & 0x0FFF);

            if (offset < nsfBankData.size())
                return nsfBankData[offset];

            return 0x00;
        }

        if (nsfMode && nsfExpansionRead)
        {
            if (nsfExpansionRead(addr, data))
                return data;
        }

        if (nsfMode && addr >= 0x4020)
        {
            return nsfMemory[addr];
        }

        if (cart != nullptr && cart->cpuRead(addr, data)) {
            open_bus = data;
            return data;
        }

        if (addr >= 0x0000 && addr <= 0x1FFF) {
            data = ram[addr & 0x07FF];
        }
        else if (addr == 0x4016) {
            // tay 1
            if (controller_strobe) {
                data = (controller_state & 0x80) ? 1 : 0;
            }
            else {
                data = (controller_shift & 0x80) ? 1 : 0;
                controller_shift <<= 1;
            }

            data |= 0x40;
        }
        else if (addr == 0x4017) {
            if (zapperConnected) {
                data = 0x40;
                data |= zapperTrigger ? 0x10 : 0x00;

                bool sensedLight = false;
                if (ppu != nullptr && zapperX >= 0 && zapperY >= 0) {
                    uint32_t pixel = ppu->GetPixelAt(zapperX, zapperY);
                    int r = (pixel >> 16) & 0xFF;
                    int g = (pixel >> 8) & 0xFF;
                    int b = pixel & 0xFF;
                    sensedLight = (r + g + b) > 300;
                }
                data |= sensedLight ? 0x00 : 0x08;
            }
            else {
                if (controller_strobe) {
                    data = (controller_state2 & 0x80) ? 1 : 0;
                }
                else {
                    data = (controller_shift2 & 0x80) ? 1 : 0;
                    controller_shift2 <<= 1;
                }
                data |= 0x40;
            }
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu != nullptr)
                data = ppu->cpuRead(addr & 0x0007);
        }
        else if (addr == 0x4015) {
            data = n_apu.readStatus();
        }

        // Byte vừa thực sự xuất hiện trên bus dữ liệu -> lưu lại làm open bus mới.
        open_bus = data;
        return data;
    }

    // =========================================================
    // HÀM CLOCK ĐIỀU PHỐI NHỊP CPU/PPU VÀ DMA CHUẨN XÁC
    // =========================================================
    // Trong file Bus.h, sửa lại hàm clock()
    void clock() {
        // 1. CPU xử lý trước
        if (system_clock_counter % 3 == 0) {
            if (dma_transfer) {
                // ... (Giữ nguyên logic DMA cũ của bạn)
            }
            else {
                if (cpu != nullptr) {
                    cpu->clock();
                    n_apu.Step();
                }
            }

            // 2. Mapper đếm nhịp
            if (cart != nullptr && cart->pMapper != nullptr) {
                cart->pMapper->clock();
            }
        }

        // 3. PPU chạy sau cùng để đảm bảo đồng bộ trạng thái
        if (ppu != nullptr) ppu->Step();

        // [Fix IRQ]: Đưa về dạng Level-Triggered chuẩn
        // Xử lý IRQ Level-Triggered:
        if (cart != nullptr && cart->pMapper != nullptr) {
            if (cart->pMapper->irqState()) {
                if (cpu != nullptr) {
                    cpu->SetIrqSource(CPU6502::IRQ_EXTERNAL);

                    // [THÊM MỚI]: Báo cho PPU biết Mapper vừa yêu cầu ngắt.
                    // Nếu PPU của bạn có hàm đặt cờ update VRAM/Nametable, hãy gọi nó ở đây.
                    // Ví dụ: if (ppu != nullptr) ppu->ForceUpdate(); 
                }
            }
            else {
                if (cpu != nullptr) cpu->ClearIrqSource(CPU6502::IRQ_EXTERNAL);
            }
        }

        system_clock_counter++;
    }
};