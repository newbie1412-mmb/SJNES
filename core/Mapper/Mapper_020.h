#pragma once
#include "Mapper.h"
#include <vector>
#include <cstdint>

// ============================================================================
// Mapper 020 - Famicom Disk System (FDS)
// ============================================================================
// PHIÊN BẢN NÀY PORT TRỰC TIẾP TỪ SOURCE THẬT CỦA MESEN (Core/FDS.cpp,
// SourMesen/Mesen trên GitHub, đã verify qua rất nhiều game trong nhiều năm),
// sau khi mọi hướng tự suy luận trước đó đều thất bại qua nhiều vòng debug.
//
// PHÁT HIỆN MẤU CHỐT làm thay đổi hoàn toàn kiến trúc:
// Mesen KHÔNG hề tổng hợp "physical stream" với gap/marker/CRC chèn sẵn như
// cách cũ (BuildPhysicalSide cũ của mình). Nó đọc THẲNG dữ liệu thô, nguyên
// văn từ file .fds (block nối liền nhau, không chèn gì thêm). "Gap" hoàn
// toàn là một LATCH PHẦN MỀM (_gapEnded), được set khi:
//   - Bit6 $4025 (_diskReady, KHÔNG PHẢI "CRC Enabled" như wiki mình đọc
//     nhầm trước đó) đang bật, VÀ
//     byte hiện tại đọc được từ đĩa KHÁC 0.
// Vị trí đọc (_diskPosition) luôn tăng đều 1 byte mỗi ~150 CPU cycle SUỐT
// khi motor bật, HOÀN TOÀN không phụ thuộc gap/block/CRC gì cả - đơn giản
// hơn nhiều so với state machine phức tạp mình tự dựng trước đó.
//
// BIT-MAPPING $4025 THEO MESEN (KHÁC với cách đọc NESdev wiki trước đó,
// nay xác nhận Mesen đúng vì đã được cộng đồng kiểm chứng qua hàng trăm game):
//   bit0 = Motor ON (1=on)                    [cũ: đọc nhầm là "Scan Disk"]
//   bit1 = Reset Transfer (1=reset/tạm dừng)  [cũ: đọc nhầm là "Motor Stop"]
//   bit2 = Read Mode (1=read, 0=write)
//   bit3 = Mirroring
//   bit4 = CRC Control
//   bit6 = Disk Ready (1=ready)                [cũ: đọc nhầm là "CRC Enabled"]
//   bit7 = Disk IRQ Enabled
//
// LỊCH SỬ DEBUG (để tránh lặp lại các hướng đã thử-và-sai trước khi có source
// Mesen thật để đối chiếu):
//   1. Rewind diskPos về 0 mỗi lần toggle bit -> ERR.23.
//   2. Gap cố định theo "spec" tự đọc (sai) -> ERR.22 liên tục, mỗi lần
//      theo một hướng khác (nhồi gap, freeze theo cycle đo được, state
//      machine chờ "cạnh lên CRC enable") đều chỉ vá được MỘT phần, vì gốc
//      rễ là kiến trúc "tổng hợp physical stream" sai hoàn toàn ngay từ đầu.
// ============================================================================

class Mapper_020 : public Mapper
{
public:
    Mapper_020(uint32_t prgBanks, uint32_t chrBanks);
    ~Mapper_020() override = default;

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool cpuReadRegister(uint16_t addr, uint8_t& data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    MIRROR mirror() override { return mirrorMode; }

    void irqStep() override;
    bool irqState() override;
    void irqClear() override;

    std::string GetDebugInfo() override;

    void SetBIOS(const std::vector<uint8_t>& biosData);
    void SetDiskSides(std::vector<std::vector<uint8_t>> sides);
    void InsertDisk(int sideIndex);
    void EjectDisk() { InsertDisk(-1); }
    int  GetCurrentSide() const { return currentSide; }
    int  GetSideCount() const { return (int)diskSides.size(); }

    uint8_t ReadBios(uint16_t addr) const;

private:
    std::vector<uint8_t> bios;

    MIRROR mirrorMode = VERTICAL;

    // ---- Timer IRQ ($4020-$4022) ----
    uint16_t irqReloadValue = 0;
    uint16_t irqCounter = 0;
    bool     irqTimerEnabled = false;
    bool     irqTimerRepeat = false;
    bool     irqTimerPending = false;

    // ---- Master I/O enable ($4023) ----
    bool diskRegsEnabled = false;
    bool soundRegsEnabled = false;

    // ---- FDS Control ($4025) - bit-mapping theo Mesen (xem giải thích ở
    // đầu file) ----
    bool motorOn = false;          // bit0
    bool resetTransfer = false;    // bit1
    bool readMode = true;          // bit2
    bool crcControl = false;       // bit4
    bool diskReady = false;        // bit6 - latch phần mềm điều khiển gap
    bool diskIrqEnabled = false;   // bit7

    // ---- Dữ liệu đĩa: LƯU THẲNG dữ liệu thô từ file .fds, không tổng hợp
    // gì thêm (khác hoàn toàn thiết kế cũ) ----
    std::vector<std::vector<uint8_t>> diskSides; // mỗi side là mảng byte thô
    int currentSide = -1;

    // ---- Trạng thái transfer (port trực tiếp từ Mesen::ProcessCpuClock) ----
    bool   endOfHead = true;
    bool   scanningDisk = false;
    bool   gapEnded = false;
    bool   previousCrcControlFlag = false;
    bool   transferComplete = false;
    int    delay = 0;
    size_t diskPosition = 0;
    uint16_t crcAccumulator = 0;

    uint8_t readDataReg = 0;
    uint8_t writeDataLatch = 0;
    bool    diskIrqPending = false;

    void UpdateCrc(uint8_t value);
    void ClockDiskTransfer();
};