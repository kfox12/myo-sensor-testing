#include <Arduino.h>

#include "emg_profiles.h"

// Kept in flash: the Uno only has 2 KB of RAM.
//
// TODO: every entry uses the same placeholder factors until the developer
// data collection runs (rest / repeated movement / sustained max x 3 trials
// per location) give real numbers.
static const LocationProfile PROFILES[] PROGMEM = {
    // name                 noise  on    off   minSnr artifact
    {"Generic",             1.4f, 0.15f, 0.08f, 3.0f, 3.0f},
    {"Forearm flexors",     1.4f, 0.15f, 0.08f, 3.0f, 3.0f},
    {"Forearm extensors",   1.4f, 0.15f, 0.08f, 3.0f, 3.0f},
    {"Biceps",              1.4f, 0.15f, 0.08f, 3.0f, 3.0f},
};

uint8_t emgProfileCount() {
    return sizeof(PROFILES) / sizeof(PROFILES[0]);
}

bool emgProfileGet(uint8_t id, LocationProfile &out) {
    if (id >= emgProfileCount()) {
        return false;
    }
    memcpy_P(&out, &PROFILES[id], sizeof(LocationProfile));
    return true;
}
