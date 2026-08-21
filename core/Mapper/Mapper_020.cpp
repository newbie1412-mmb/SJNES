#include "Mapper_020.h"
#include <QDebug>

Mapper_020::Mapper_020(uint32_t /*prgBanks*/, uint32_t /*chrBanks*/)
    : Mapper(0, 0)
{
    reset();
}

void Mapper_020::reset()
{
    irqReloadValue = 0;
    irqCounter = 0;
    irqTimerEnabled = false;
    irqTimerRepeat = false;
    irqTimerPending = false;

    diskRegsEnabled = false;
    soundRegsEnabled = false;

    motorOn = false;
    resetTransfer = false;
    readMode = true;
    crcControl = false;
    diskReady = false;
    diskIrqEnabled = false;

    // Không reset currentSide ở đây: đĩa vẫn "nằm trong ổ" xuyên suốt reset
    // (giống hardware thật), chỉ InsertDisk()/EjectDisk() mới đổi việc này.
    endOfHead = true;   // giống Mesen::Reset - buộc phiên đọc bắt đầu lại từ
    // đầu side (delay 50000 cycle) ở lần clock kế tiếp
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;

    readDataReg = 0;
    writeDataLatch = 0;
    diskIrqPending = false;

    mirrorMode = VERTICAL;
}

bool Mapper_020::cpuMapRead(uint16_t /*addr*/, uint32_t& /*mapped_addr*/)
{
    return false;
}

bool Mapper_020::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data)
{
    mapped_addr = 0;

    if (addr >= 0x4020 && addr <= 0x4026)
        qDebug("[FDS] W $%04X = $%02X", addr, data);

    // Port từ Mesen::FDS::WriteRegister - $4024-$4026 chỉ có tác dụng khi
    // diskRegsEnabled (Cartridge đã lọc $6000-$FFFF trước khi vào đây, nên
    // addr vào hàm này chỉ có thể là $4020-$4026).
    if (!diskRegsEnabled && addr >= 0x4024 && addr <= 0x4026)
        return true;

    switch (addr)
    {
    case 0x4020:
        irqReloadValue = (irqReloadValue & 0xFF00) | uint16_t(data);
        return true;

    case 0x4021:
        irqReloadValue = (irqReloadValue & 0x00FF) | (uint16_t(data) << 8);
        return true;

    case 0x4022:
        irqTimerRepeat = (data & 0x01) != 0;
        irqTimerEnabled = (data & 0x02) != 0 && diskRegsEnabled;
        if (irqTimerEnabled)
            irqCounter = irqReloadValue;
        else
            irqTimerPending = false;
        return true;

    case 0x4023:
        diskRegsEnabled = (data & 0x01) != 0;
        soundRegsEnabled = (data & 0x02) != 0;
        if (!diskRegsEnabled)
        {
            irqTimerEnabled = false;
            irqTimerPending = false;
            diskIrqPending = false;
        }
        return true;

    case 0x4024:
        writeDataLatch = data;
        transferComplete = false;
        // Mesen: "Unsure về việc clear irq ở đây: FCEUX/Nintendulator không
        // làm, puNES có làm" - Mesen chọn clear, mình theo Mesen.
        diskIrqPending = false;
        return true;

    case 0x4025:
    {
        motorOn = (data & 0x01) != 0;
        resetTransfer = (data & 0x02) != 0;
        readMode = (data & 0x04) != 0;
        mirrorMode = (data & 0x08) ? HORIZONTAL : VERTICAL;
        crcControl = (data & 0x10) != 0;
        // bit5 không dùng, luôn =1 theo BIOS/game thật
        diskReady = (data & 0x40) != 0;
        diskIrqEnabled = (data & 0x80) != 0;

        // Ghi $4025 luôn clear disk IRQ (theo Mesen, khớp FCEUX/puNES/
        // Nintendulator - sửa lỗi ERR.20 lúc power-on ở một số game
        // unlicensed theo comment gốc của Mesen).
        diskIrqPending = false;
        return true;
    }

    case 0x4026:
        return true;
    }

    return false;
}

bool Mapper_020::cpuReadRegister(uint16_t addr, uint8_t& data)
{
    if (!diskRegsEnabled || addr > 0x4033)
        return false;

    switch (addr)
    {
    case 0x4030:
    {
        // Port từ Mesen: chỉ có bit0 (timer IRQ), bit1 (transfer complete),
        // bit4 (bad CRC) là có ý nghĩa thật - các bit khác là open bus trên
        // hardware thật (Mesen giữ nguyên giá trị bus cũ cho các bit đó).
        // Mô hình đơn giản hoá của mình không track open bus nên để 0.
        uint8_t v = 0;
        if (irqTimerPending) v |= 0x01;
        if (transferComplete) v |= 0x02;
        // bit4 "bad CRC": mô hình đơn giản hoá CRC luôn "passed" (0) - chưa
        // implement kiểm tra CRC thật.

        data = v;

        transferComplete = false;
        irqTimerPending = false;
        diskIrqPending = false;

        qDebug("[FDS] R $4030 = $%02X (pos=%zu side=%d)", v, diskPosition, currentSide);
        return true;
    }

    case 0x4031:
        transferComplete = false;
        diskIrqPending = false;
        data = readDataReg;
        qDebug("[FDS] R $4031 = $%02X (pos=%zu)", readDataReg, diskPosition);
        return true;

    case 0x4032:
    {
        // Port từ Mesen: Ready flag phụ thuộc "scanningDisk" (đã thực sự bắt
        // đầu xử lý transfer), KHÔNG phải "motor đang bật hay tắt" như cách
        // mình từng làm sai trước đó.
        uint8_t v = 0;
        if (currentSide < 0) v |= 0x01;                       // không có đĩa
        if (currentSide < 0 || !scanningDisk) v |= 0x02;       // chưa sẵn sàng
        if (currentSide < 0) v |= 0x04;                        // không ghi được (chỉ khi không có đĩa - theo Mesen)

        data = v;
        return true;
    }

    case 0x4033:
        data = 0x80; // luôn báo battery tốt
        return true;
    }

    return false;
}

bool Mapper_020::ppuMapRead(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        mapped_addr = addr;
        return true;
    }
    return false;
}

bool Mapper_020::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr)
{
    if (addr < 0x2000)
    {
        mapped_addr = addr;
        return true;
    }
    return false;
}

void Mapper_020::irqStep()
{
    if (irqTimerEnabled)
    {
        if (irqCounter == 0)
        {
            irqTimerPending = true;
            irqCounter = irqReloadValue;
            if (!irqTimerRepeat)
                irqTimerEnabled = false;
        }
        else
        {
            irqCounter--;
        }
    }

    // QUAN TRỌNG (fix sau khi đối chiếu source Mesen thật): ClockDiskTransfer()
    // PHẢI được gọi VÔ ĐIỀU KIỆN mỗi CPU cycle, KHÔNG được gate bởi
    // diskRegsEnabled. Trong Mesen::FDS::ProcessCpuClock(), phần xử lý motor/
    // disk position hoàn toàn không kiểm tra _diskRegEnabled - biến đó CHỈ
    // chặn việc ĐỌC/GHI thanh ghi ($4024-$4033), không liên quan gì tới việc
    // motor/vị trí đĩa có được clock hay không. Bản trước của mình gate nhầm
    // bằng "if (diskRegsEnabled)" ở đây - đây là nguồn gốc thật của toàn bộ
    // hiện tượng "overshoot" (vị trí đọc luôn vượt quá xa so với đúng) đã
    // thấy xuyên suốt mọi lần viết lại mapper trước đó.
    ClockDiskTransfer();
}

bool Mapper_020::irqState()
{
    return irqTimerPending || diskIrqPending;
}

void Mapper_020::irqClear()
{
    irqTimerPending = false;
    diskIrqPending = false;
}

// Port trực tiếp từ Mesen::UpdateCrc (CRC-16/CCITT kiểu FDS dùng)
void Mapper_020::UpdateCrc(uint8_t value)
{
    for (uint16_t n = 0x01; n <= 0x80; n <<= 1)
    {
        uint8_t carry = (crcAccumulator & 1);
        crcAccumulator >>= 1;
        if (carry)
            crcAccumulator ^= 0x8408;
        if (value & n)
            crcAccumulator ^= 0x8000;
    }
}

// Port trực tiếp từ Mesen::ProcessCpuClock (phần xử lý disk transfer).
// Đây là thay đổi CỐT LÕI so với mọi phiên bản trước: không còn state
// machine gap/block/CRC tự dựng - chỉ đơn giản đọc byte thô tại diskPosition,
// tăng dần đều, và dùng "gapEnded" latch mềm để quyết định khi nào dữ liệu
// thật sự "xuất hiện" cho CPU.
void Mapper_020::ClockDiskTransfer()
{
    if (currentSide < 0 || !motorOn)
    {
        endOfHead = true;
        scanningDisk = false;
        return;
    }

    if (resetTransfer && !scanningDisk)
        return;

    if (endOfHead)
    {
        delay = 50000;
        endOfHead = false;
        diskPosition = 0;
        gapEnded = false;
        return;
    }

    if (delay > 0)
    {
        delay--;
        return;
    }

    scanningDisk = true;

    auto& side = diskSides[size_t(currentSide)];
    if (diskPosition >= side.size())
    {
        motorOn = false;
        return;
    }

    uint8_t diskData = 0;
    bool needIrq = diskIrqEnabled;

    if (readMode)
    {
        diskData = side[diskPosition];
        if (!previousCrcControlFlag)
            UpdateCrc(diskData);

        if (!diskReady)
        {
            gapEnded = false;
            crcAccumulator = 0;
        }
        else if (diskData != 0 && !gapEnded)
        {
            gapEnded = true;
            needIrq = false;
        }

        if (gapEnded)
        {
            transferComplete = true;
            readDataReg = diskData;
            if (needIrq)
                diskIrqPending = true;
        }
    }
    else
    {
        // Write mode: KHÔNG persist ra file .fds thật (chỉ giữ tạm trong
        // RAM phiên chơi) - xem ghi chú ở $4032 cũ, giờ đơn giản hoá bỏ hẳn
        // write-protect cứng, theo đúng Mesen (chỉ báo not-writable khi
        // không có đĩa).
        if (!crcControl)
        {
            transferComplete = true;
            diskData = writeDataLatch;
            if (needIrq)
                diskIrqPending = true;
        }
        if (!diskReady)
            diskData = 0x00;

        if (!crcControl)
        {
            UpdateCrc(diskData);
        }
        else
        {
            if (!previousCrcControlFlag)
            {
                UpdateCrc(0x00);
                UpdateCrc(0x00);
            }
            diskData = uint8_t(crcAccumulator & 0xFF);
            crcAccumulator >>= 8;
        }

        // Mesen ghi lệch 2 byte về phía sau (đầu ghi thật sự chậm hơn đầu đọc
        // 2 byte do độ trễ tính CRC) - giữ nguyên hành vi để tương thích, dù
        // dữ liệu ghi không persist ra file thật.
        if (diskPosition >= 2 && diskPosition - 2 < side.size())
            side[diskPosition - 2] = diskData;

        gapEnded = false;
    }

    previousCrcControlFlag = crcControl;
    diskPosition++;

    if (diskPosition >= side.size())
    {
        motorOn = false;
    }
    else
    {
        delay = 150;
    }
}

void Mapper_020::SetBIOS(const std::vector<uint8_t>& biosData)
{
    bios = biosData;
    bios.resize(8192, 0xFF);
}

uint8_t Mapper_020::ReadBios(uint16_t addr) const
{
    if (bios.empty())
        return 0xFF;

    uint32_t offset = uint32_t(addr) - 0xE000;
    if (offset < bios.size())
        return bios[offset];

    return 0xFF;
}

void Mapper_020::SetDiskSides(std::vector<std::vector<uint8_t>> sides)
{
    // Lưu THẲNG dữ liệu thô, không tổng hợp gap/marker/CRC gì thêm (khác
    // hoàn toàn thiết kế cũ - xem giải thích ở đầu Mapper_020.h).
    diskSides = std::move(sides);
    currentSide = diskSides.empty() ? -1 : 0;

    endOfHead = true;
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;
}

void Mapper_020::InsertDisk(int sideIndex)
{
    if (sideIndex < 0 || sideIndex >= int(diskSides.size()))
        currentSide = -1;
    else
        currentSide = sideIndex;

    endOfHead = true;
    scanningDisk = false;
    gapEnded = false;
    previousCrcControlFlag = false;
    transferComplete = false;
    delay = 0;
    diskPosition = 0;
    crcAccumulator = 0;
}

std::string Mapper_020::GetDebugInfo()
{
    std::string s;
    s += "===== Famicom Disk System / Mapper 020 =====\n";
    s += "Disk sides: " + std::to_string(diskSides.size()) + "\n";
    s += "Current side: " + std::to_string(currentSide) + "\n";
    s += "Disk position: " + std::to_string(diskPosition) + "\n";
    s += "Motor: " + std::string(motorOn ? "ON" : "OFF") + "\n";
    s += "Scanning: " + std::string(scanningDisk ? "YES" : "NO") + "\n";
    s += "Transfer mode: " + std::string(readMode ? "READ" : "WRITE") + "\n";
    s += "Disk regs enabled: " + std::string(diskRegsEnabled ? "YES" : "NO") + "\n";
    s += "BIOS loaded: " + std::string(bios.empty() ? "NO" : "YES") + "\n";
    return s;
}