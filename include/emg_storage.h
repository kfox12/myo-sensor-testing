#pragma once

#include <stdint.h>

// A user's calibration result, persisted in EEPROM so it survives power
// cycles. Fixed-width fields keep the layout identical on every compiler.

#define CALIBRATION_MAGIC 0x454D // "EM"
#define CALIBRATION_VERSION 1

struct UserCalibration {
    uint16_t magic;
    uint8_t version;
    uint8_t locationId;     // index into the profile table (emg_profiles.h)
    int32_t noiseThreshold; // group power at or below this is reported as 0
    int32_t mvcPower;       // mean power of a max contraction; 0 = uncalibrated
    int32_t onThreshold;    // activation on threshold (power)
    int32_t offThreshold;   // activation off threshold (power)
    int32_t artifactLimit;  // group power above this is a sensor bump
    uint16_t checksum;      // over every byte before this field
};

// Checksum of everything in `cal` before the checksum field.
uint16_t emgStorageChecksum(const UserCalibration &cal);

// Read the stored calibration into `out`.
// Returns false if EEPROM holds no valid calibration (bad magic, version,
// or checksum); `out` is then undefined and the caller should use defaults.
bool emgStorageLoad(UserCalibration &out);

// Stamp magic / version / checksum into `cal` and write it to EEPROM.
void emgStorageSave(UserCalibration &cal);

// Invalidate the stored calibration so the next load fails.
void emgStorageErase();
