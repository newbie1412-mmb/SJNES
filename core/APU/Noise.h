#pragma once
#include <cstdint>
#include "ApuEnvelope.h"

static const uint16_t noise_period_table[16] = {
    4,8,16,32,64,96,128,160,
    202,254,380,508,762,1016,2034,4068
};

static const uint8_t noise_length_table[32] = {
    10,254,20, 2,40, 4,80, 6,
   160,  8,60,10,14,12,26,14,
    12, 16,24,18,48,20,96,22,
   192, 24,72,26,16,28,32,30
};

struct Noise {
    uint16_t    timer          = 0;
    uint16_t    timer_reload   = 0;
    uint16_t    shift_register = 1;
    uint8_t     length_counter = 0;
    bool        mode           = false;
    bool        enabled        = false;
    ApuEnvelope env;

    // Gọi mỗi CPU cycle
    void TickTimer() {
        if (!enabled) return;
        if (timer > 0) {
            timer--;
        } else {
            timer = timer_reload;
            int      bit = mode ? 6 : 1;
            uint16_t fb  = (shift_register & 1) ^
                           ((shift_register >> bit) & 1);
            shift_register = (shift_register >> 1) | (fb << 14);
        }
    }

    // Gọi ở quarter-frame
    void TickEnvelope() {
        env.Clock();
    }

    // Gọi ở half-frame
    void TickLengthCounter() {
        if (!env.loop && length_counter > 0)
            length_counter--;
    }

    float Output() const {
        if (!enabled)            return 0.0f;
        if (length_counter == 0) return 0.0f;
        if (shift_register & 1)  return 0.0f; // bit 0 = 1 → im lặng
        return (float)env.Output();
    }

    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:  // $400C
            env.loop            = (data & 0x20) != 0;
            env.constant_volume = (data & 0x10) != 0;
            env.volume          = data & 0x0F;
            break;
        case 1:  // $400E
            mode         = (data & 0x80) != 0;
            timer_reload = noise_period_table[data & 0x0F];
            break;
        case 2:  // $400F
            if (enabled) length_counter = noise_length_table[(data & 0xF8) >> 3];
            timer     = timer_reload;
            env.start = true;
            break;
        }
    }
};
