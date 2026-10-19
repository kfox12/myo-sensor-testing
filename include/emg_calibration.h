#pragma once

#include <stdint.h>

#include "emg_envelope.h"
#include "emg_profiles.h"
#include "emg_storage.h"

// Guided per-user calibration.
//
// Turns the selected location profile's relative factors into absolute
// thresholds for this user and session and saves them to EEPROM. It only
// measures and reports: the envelope's power output is never changed.
// Sequence (about 19 s):
//   settle 2 s -> rest 8 s -> hold 3 s -> relax 3 s -> hold 3 s -> save
//
// It is non-blocking: feed it every finished group with
// emgCalibrationAdd(), so 1 kHz sampling never stops. While it runs it
// owns the stats module (emg_stats.h); don't call emgStatsAdd() then.

// Load the saved calibration (or defaults). Call once from setup().
void emgCalibrationInit();

// Begin a calibration run for the selected location.
// Returns false if one is already running.
bool emgCalibrationStart();

bool emgCalibrationRunning();

// Feed one finished group from emgEnvelopeUpdate(). Ignored when idle.
void emgCalibrationAdd(const EmgEnvelope &group);

// Choose the location profile the next calibration run uses.
// Returns false if `id` is out of range or a run is in progress.
bool emgCalibrationSelectLocation(uint8_t id);

// Print the active calibration over Serial.
void emgCalibrationPrint();

// Erase the saved calibration and go back to defaults (uncalibrated).
// Returns false if a run is in progress.
bool emgCalibrationErase();

// Express `power` as % (0-100) of the calibrated max contraction.
// Returns 0 when uncalibrated.
int emgCalibrationActivation(long power);

// --- Pure computation, exposed for host-side testing ---

enum CalibrationResult {
    CAL_OK,
    CAL_CLIPPED,            // ADC hit its rails: sensor gain too high
    CAL_TOO_MANY_ARTIFACTS, // too many groups lost to sensor bumps
    CAL_LOW_SNR,            // max contraction too close to the rest floor
};

// Check a run's measurements against the profile's sanity limits.
// `recordedGroups` / `clippedGroups` / `suppressedGroups` are totals over
// the rest and hold phases.
CalibrationResult emgCalibrationValidate(const LocationProfile &profile,
                                         long restP99, long mvcPower,
                                         unsigned long recordedGroups,
                                         unsigned long clippedGroups,
                                         unsigned long suppressedGroups);

// Derive absolute thresholds from the rest floor and max contraction.
// Fills every field of `out` except magic / version / checksum.
void emgCalibrationCompute(const LocationProfile &profile, uint8_t locationId,
                           long restP99, long mvcPower, UserCalibration &out);
