#pragma once
#include <cstdint>
#include "ApuEnvelope.h"
#include "F1.h"   // dùng chung sq_duty_table, sq_length_table

struct F2 {
    uint16_t    timer          = 0;
    uint16_t    timer_reload   = 0;
    uint8_t     duty           = 0;
    uint8_t     pulse_phase    = 0;
    uint8_t     length_counter = 0;
    bool        enabled        = false;
    ApuEnvelope env;

    struct Sweep {
        bool    enabled = false;
        bool    negate  = false;
        bool    reload  = false;
        uint8_t shift   = 0;
        uint8_t period  = 0;
        uint8_t timer   = 0;
    } sweep;

    void TickTimer() {
        if (!enabled) return;
        if (timer > 0) timer--;
        else {
            timer       = timer_reload;
            pulse_phase = (pulse_phase + 1) % 8;
        }
    }

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
                    ? (timer_reload - change)       // Pulse 2
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
        int target = sweep.negate ? (timer_reload - change)
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

    void WriteReg(uint8_t reg, uint8_t data) {
        switch (reg) {
        case 0:
            duty                = (data & 0xC0) >> 6;
            env.loop            = (data & 0x20) != 0;
            env.constant_volume = (data & 0x10) != 0;
            env.volume          = data & 0x0F;
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
