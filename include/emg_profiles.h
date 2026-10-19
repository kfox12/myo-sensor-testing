#pragma once

#include <stdint.h>

// Per-location sensor characteristics, measured once by developers.
//
// These are *relative* factors (ratios to a user's own rest noise and max
// contraction), so they transfer between people. The absolute thresholds
// come from each user's calibration run (see emg_calibration.h).

#define LOCATION_GENERIC 0
#define LOCATION_NAME_LENGTH 18

struct LocationProfile {
    char name[LOCATION_NAME_LENGTH];
    float noiseFactor;    // noise threshold = noiseFactor x rest p99
    float onFraction;     // activation on threshold = onFraction x max contraction
    float offFraction;    // activation off threshold = offFraction x max contraction
    float minSnr;         // reject calibration if max contraction / rest p99 is below this
    float artifactFactor; // artifact limit = artifactFactor x max contraction
};

// Number of entries in the profile table.
uint8_t emgProfileCount();

// Copy profile `id` out of flash into `out`.
// Returns false (and leaves `out` alone) if `id` is out of range.
bool emgProfileGet(uint8_t id, LocationProfile &out);
