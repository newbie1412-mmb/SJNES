#pragma once
#include <cstdint>
#include "ApuEnvelope.h"

// Duty table chuẩn NES
static const uint8_t sq_duty_table[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};

static const uint8_t sq_length_table[32] = {
    10,254,20, 2,40, 4,80, 6,
   160,  8,60,10,14,12,26,14,
    12, 16,24,18,48,20,96,22,
   192, 24,72,26,16,28,32,30
};

struct F1 {
    uint16_t    timer         = 0;
    uint16_t    timer_reload  = 0;
    uint8_t     duty          = 0;
    uint8_t     pulse_phase   = 0;
    uint8_t     length_counter= 0;
    bool        enabled       = false;
    ApuEnvelope env;

    struct Sweep {
        bool    enabled = false;
        bool    negate  = false;
        bool    reload  = false;
        uint8_t shift   = 0;
        uint8_t period  = 0;
        uint8_t timer   = 0;
    } sweep;

    // Gọi mỗi 2 CPU cycles (half-clock)
    void TickTimer() {
        if (!enabled) return;
        if (timer > 0) timer--;
        else {
            timer       = timer_reload;
            pulse_phase = (pulse_phase + 1) % 8;
        }
    }

    // Gọi ở half-frame
    void TickLengthAndSweep()
    {
        if (!env.loop && length_counter > 0)
            length_counter--;

        if (sweep.timer > 0)
            sweep.timer--;

        if (sweep.timer == 0)
        {
            if (sweep.enabled && sweep.shift > 0 && length_counter > 0 && !IsMuted())
            {
                int change = timer_reload >> sweep.shift;
                int target = sweep.negate
                    ? (timer_reload - change - 1)   // Pulse 1
                    : (timer_reload + change);

                if (target >= 0 && target <= 0x7FF)
                    timer_reload = (uint16_t)target;
            }

            sweep.timer = sweep.period;
        }

        if (sweep.reload)
        {
            sweep.timer = sweep.period;
            sweep.reload = false;
        }
    }

    bool IsMuted() const {
        if (timer_reload < 8) return true;
        int change = sweep.shift ? (timer_reload >> sweep.shift) : 0;
        int target = sweep.negate ? (timer_reload - change - 1)
                                  : (timer_reload + change);
        return (target > 0x7FF || target < 0);
    }

    float Output() const {
        if (!enabled)            return 0.0f;
        if (IsMuted())           return 0.0f;
        if (length_counter == 0) return 0.0f;
        if (!sq_duty_table[duty][pulse_phase]) return 0.0f;
        return (float)env.Output();
    }

    // Viết thanh ghi $4000-$4003
    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:
            duty                  = (data & 0xC0) >> 6;
            env.loop              = (data & 0x20) != 0;
            env.constant_volume   = (data & 0x10) != 0;
            env.volume            = data & 0x0F;
            break;
        case 1:
            sweep.enabled = (data & 0x80) != 0;
            sweep.period = ((data >> 4) & 0x07) + 1;
            sweep.negate  = (data & 0x08) != 0;
            sweep.shift   = data & 0x07;
            sweep.reload  = true;
            break;
        case 2:
            timer_reload = (timer_reload & 0xFF00) | data;
            break;
        case 3:
            timer_reload = ((uint16_t)(data & 0x07) << 8) | (timer_reload & 0x00FF);
            timer        = timer_reload;
            if (enabled) length_counter = sq_length_table[(data & 0xF8) >> 3];
            pulse_phase  = 0;
            env.start    = true;
            break;
        }
    }
};
