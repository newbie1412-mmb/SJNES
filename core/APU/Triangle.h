#pragma once
#include <cstdint>

static const uint8_t tri_length_table[32] = {
    10,254,20, 2,40, 4,80, 6,
   160,  8,60,10,14,12,26,14,
    12, 16,24,18,48,20,96,22,
   192, 24,72,26,16,28,32,30
};

// Bảng 32 bước chuẩn NES
static const float tri_wave_table[32] = {
    15,14,13,12,11,10,9,8,
     7, 6, 5, 4, 3, 2,1,0,
     0, 1, 2, 3, 4, 5,6,7,
     8, 9,10,11,12,13,14,15
};

struct Triangle {
    uint16_t timer = 0;
    uint16_t timer_reload = 0;
    uint8_t  tri_phase = 0;
    uint8_t  length_counter = 0;
    uint8_t  linear_counter = 0;
    uint8_t  linear_reload_value = 0;
    bool     linear_reload_flag = false;
    bool     control_flag = false;   // bit 7 của $4008
    bool     enabled = false;
    bool     smooth = false; // false: staircase thô (NES thật); true: nội suy mượt cho AUDIO
    mutable float last_output = 0.0f;  // giữ giá trị cuối để tránh pop khi tắt

    // Gọi mỗi CPU cycle
    void TickTimer() {
        if (!enabled) return;
        if (length_counter == 0 || linear_counter == 0) return;

        if (timer > 0) {
            timer--;
        }
        else {
            timer = timer_reload;
            // timer_reload < 2: dừng advance (tránh ultrasonic buzz)
            // nhưng KHÔNG reset phase → không pop
            if (timer_reload >= 2)
                tri_phase = (tri_phase + 1) & 31;
        }
    }

    // Gọi ở quarter-frame
    void TickLinearCounter() {
        if (linear_reload_flag)
            linear_counter = linear_reload_value;
        else if (linear_counter > 0)
            linear_counter--;

        if (!control_flag)
            linear_reload_flag = false;
    }

    // Gọi ở half-frame
    void TickLengthCounter() {
        if (!control_flag && length_counter > 0)
            length_counter--;
    }
    float Output() const {
        if (!enabled) return last_output;

        if (!smooth) {
            // Staircase thô, đúng kiểu NES thật (32 bậc, không nội suy).
            last_output = tri_wave_table[tri_phase];
            return last_output;
        }

        // Nội suy tuyến tính giữa bậc hiện tại và bậc kế tiếp,
        // dựa trên vị trí của timer trong period hiện tại.
        // timer đếm ngược từ timer_reload -> 0, khi về 0 mới advance phase.
        uint8_t next_phase = (tri_phase + 1) & 31;
        float frac = 0.0f;
        if (timer_reload > 0) {
            frac = (float)(timer_reload - timer) / (float)(timer_reload + 1);
        }

        float v0 = tri_wave_table[tri_phase];
        float v1 = tri_wave_table[next_phase];

        last_output = v0 + (v1 - v0) * frac;
        return last_output;
    }
    float DebugOutput() const {
        if (!enabled || length_counter == 0 || linear_counter == 0)
            return 0.0f;

        if (!smooth) {
            float v = tri_wave_table[tri_phase & 31] / 15.0f;
            return v * 2.0f - 1.0f;
        }

        // Nội suy y hệt Output() để oscilloscope khớp với âm thanh thật khi smooth bật.
        // minSlope trong AudioWaveWindow.cpp đã được hạ thấp để chịu được ramp mượt này.
        uint8_t next_phase = (tri_phase + 1) & 31;
        float frac = 0.0f;
        if (timer_reload > 0) {
            frac = (float)(timer_reload - timer) / (float)(timer_reload + 1);
        }

        float v0 = tri_wave_table[tri_phase];
        float v1 = tri_wave_table[next_phase];
        float v = (v0 + (v1 - v0) * frac) / 15.0f;

        return v * 2.0f - 1.0f;
    }
    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:  // $4008
            control_flag = (data & 0x80) != 0;
            linear_reload_value = data & 0x7F;
            break;
        case 1:  // $400A
            timer_reload = (timer_reload & 0xFF00) | data;
            break;
        case 2:  // $400B
            timer_reload = ((uint16_t)(data & 0x07) << 8) | (timer_reload & 0x00FF);
            timer = timer_reload;
            if (enabled) length_counter = tri_length_table[(data & 0xF8) >> 3];
            linear_reload_flag = true;
            break;
        }
    }
};