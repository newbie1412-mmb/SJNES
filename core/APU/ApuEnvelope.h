#pragma once
#include <cstdint>

struct ApuEnvelope {
    bool    start           = false;
    bool    loop            = false;
    bool    constant_volume = false;
    uint8_t volume          = 0;
    uint8_t decay_level     = 0;
    uint8_t timer           = 0;

    void Clock() {
        if (start) {
            start       = false;
            decay_level = 15;
            timer       = volume;
        } else {
            if (timer > 0) {
                timer--;
            } else {
                timer = volume;
                if (decay_level > 0) decay_level--;
                else if (loop)       decay_level = 15;
            }
        }
    }

    uint8_t Output() const {
        return constant_volume ? volume : decay_level;
    }
};
