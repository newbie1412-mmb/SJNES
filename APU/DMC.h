#pragma once
#include <cstdint>
#include <functional>
#include <algorithm>

static const uint16_t dmc_rate_table[16] = {
    428,380,340,320,286,254,226,214,
    190,160,142,128,106, 84, 72, 54
};

// Bảng lookup đảo thứ tự bit trong 1 byte (LSB<->MSB), dùng cho "Reverse DPCM Bit Order".
// Một số game bên thứ 3 (Double Dribble, Gimmick!, vài game Konami...) và famiclone
// đời đầu encode/phát sample DPCM theo thứ tự bit ngược so với chuẩn hardware NES thật,
// khiến âm thanh (thường là giọng nói/trống) bị méo/rè. Bật tùy chọn này sẽ đảo bit
// khi đọc từng byte sample để bù lại, giống tính năng "Reverse Bits" trong puNES/
// NSFPlay/FamiStudio (Mesen hiện vẫn CHƯA có tính năng này tính đến thời điểm viết).
static inline uint8_t ReverseBits8(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

struct DMC {
    bool     irq_enable = false;
    bool     loop = false;
    uint8_t  rate_index = 0;
    uint16_t timer_reload = 0;
    uint16_t timer = 0;

    uint8_t  output_level = 0;
    uint16_t sample_address = 0x0000;
    uint16_t sample_length = 0;
    uint16_t current_address = 0;
    uint16_t bytes_remaining = 0;
    uint8_t  shift_register = 0;
    uint8_t  bits_remaining = 0;

    bool     enabled = false;

    // ---- Đo period lặp thật (dùng cho oscilloscope, KHÔNG ảnh hưởng audio) ----
    // Ý tưởng: mỗi lần bytes_remaining chạm 0 và loop=true, DMC tự Restart() —
    // đây là mốc lặp THẬT của hardware, chính xác tuyệt đối, không cần đoán.
    // Ta chỉ cần đếm số CPU cycle giữa 2 lần Restart() liên tiếp do loop gây ra.
    uint64_t dbgCycleCounter = 0;          // đếm cycle nội bộ, chỉ phục vụ debug/UI
    uint64_t dbgLastLoopRestartCycle = 0;  // cycle tại lần Restart() do loop gần nhất
    uint32_t dbgLoopPeriodCycles = 0;      // số cycle của 1 vòng lặp đầy đủ (0 = chưa đo được)
    bool     dbgHasLoopPeriod = false;     // true khi đã đo được ít nhất 1 chu kỳ trọn vẹn

    // Bật để đảo bit mỗi byte sample DPCM trước khi phát — dùng cho các ROM
    // encode sai thứ tự bit (xem ReverseBits8 phía trên). Mặc định TẮT vì đa số
    // game encode đúng chuẩn; chỉ bật khi người dùng phát hiện DMC bị méo/rè.
    bool     reverseBits = false;

    // Bật để giảm tiếng "pop" khi CPU ghi trực tiếp vào $4011 (output level).
    // Thay vì set output_level ngay lập tức (gây delta lớn đột ngột -> click/pop),
    // ta cho giá trị "trườn" dần tới target với bước +-2 mỗi timer clock, y hệt
    // cơ chế tăng/giảm bình thường của DMC khi phát sample. Đây đúng là cách
    // Mesen implement "Reduce popping sounds on the DMC channel".
    // Mặc định TẮT vì hardware thật KHÔNG làm vậy (pop là đặc trưng phần cứng).
    bool     reducePopping = false;
    bool     rampActive = false;
    uint8_t  rampTarget = 0;

    // Callback để đọc RAM — gán từ APU: dmc.reader = [&](uint16_t a){ return bus->cpuRead(a); };
    std::function<uint8_t(uint16_t)> reader;

    void SetEnabled(bool en) {
        enabled = en;
        if (!enabled) {
            bytes_remaining = 0;
            bits_remaining = 0;
        }
        else {
            if (bytes_remaining == 0) Restart();
        }
    }

    void Restart() {
        current_address = sample_address;
        bytes_remaining = sample_length;
        bits_remaining = 0;
        timer = timer_reload;
    }

    // Gọi mỗi CPU cycle
    void TickTimer() {
        dbgCycleCounter++;

        if (!enabled) return;

        if (timer > 0) { timer--; return; }
        timer = timer_reload;

        // Ramp output_level dần tới rampTarget (chỉ chạy khi reducePopping bật và
        // đang có 1 lần ghi $4011 trực tiếp cần làm mượt). Bước +-2 mỗi timer clock,
        // giống hệt tốc độ thay đổi bình thường của DMC nên nghe tự nhiên, không lộ.
        if (rampActive) {
            if (output_level < rampTarget) {
                output_level = (uint8_t)std::min<int>(output_level + 2, rampTarget);
            }
            else if (output_level > rampTarget) {
                output_level = (uint8_t)std::max<int>((int)output_level - 2, rampTarget);
            }
            if (output_level == rampTarget) rampActive = false;
        }

        // Output unit
        if (bits_remaining > 0) {
            if (shift_register & 1) { if (output_level <= 125) output_level += 2; }
            else { if (output_level >= 2)   output_level -= 2; }
            shift_register >>= 1;
            bits_remaining--;
        }

        // Load sample byte
        if (bits_remaining == 0 && bytes_remaining > 0 && reader) {
            uint8_t sampleByte = reader(current_address);
            if (reverseBits)
                sampleByte = ReverseBits8(sampleByte);

            shift_register = sampleByte;
            bits_remaining = 8;

            // Wrap address: uint16_t 0xFFFF+1 = 0x0000 → về 0x8000
            if (current_address == 0xFFFF)
                current_address = 0x8000;
            else
                current_address++;

            bytes_remaining--;

            if (bytes_remaining == 0 && loop) {
                Restart();

                // Mốc lặp thật: chỉ tin khi đã có lần restart trước đó
                // (tránh đo sai ở lần loop đầu tiên khi chưa có mốc cũ).
                if (dbgLastLoopRestartCycle != 0) {
                    dbgLoopPeriodCycles = (uint32_t)(dbgCycleCounter - dbgLastLoopRestartCycle);
                    dbgHasLoopPeriod = true;
                }
                dbgLastLoopRestartCycle = dbgCycleCounter;
            }
        }
    }

    float Output() const {
        return (float)output_level;
    }

    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:  // $4010
        {
            bool oldLoop = loop;
            irq_enable = (data & 0x80) != 0;
            loop = (data & 0x40) != 0;
            rate_index = data & 0x0F;
            timer_reload = dmc_rate_table[rate_index];
            if (oldLoop && !loop) {
                // Tắt loop giữa chừng -> period cũ không còn nghĩa lý gì nữa
                dbgHasLoopPeriod = false;
                dbgLastLoopRestartCycle = 0;
            }
            break;
        }
        case 1:  // $4011
        {
            uint8_t newLevel = data & 0x7F;
            if (reducePopping) {
                // Không set ngay, chỉ đặt target rồi để TickTimer() trườn dần tới đó.
                rampTarget = newLevel;
                rampActive = (rampTarget != output_level);
            }
            else {
                output_level = newLevel;
                rampActive = false; // đảm bảo tắt hẳn nếu toggle bị bật/tắt giữa chừng
            }
            break;
        }
        case 2:  // $4012
            sample_address = 0xC000 | ((uint16_t)data << 6);
            // Đổi sample -> period lặp đo được trước đó (nếu có) không còn đúng nữa
            dbgHasLoopPeriod = false;
            dbgLastLoopRestartCycle = 0;
            break;
        case 3:  // $4013
            sample_length = ((uint16_t)data << 4) + 1;
            dbgHasLoopPeriod = false;
            dbgLastLoopRestartCycle = 0;
            break;
        }
    }
};