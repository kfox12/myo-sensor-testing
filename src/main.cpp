#include <Arduino.h>

#include "emg_calibration.h"
#include "emg_envelope.h"
#include "emg_profiles.h"
#include "emg_stats.h"

#define TIMING_DEBUG 1

// skip the filters' startup transient (10 ms per group) before collecting
// statistics, so it doesn't show up as a huge max
#define STATS_WARMUP_GROUPS 50

static int warmupRemaining = STATS_WARMUP_GROUPS;

static void printHelp() {
    Serial.println(F("Commands: 's' stats, 'r' reset stats, 'c' calibrate,"));
    Serial.println(F("'i' calibration info, 'x' erase calibration,"));
    Serial.println(F("digit = select location:"));
    LocationProfile profile;
    for (uint8_t id = 0; emgProfileGet(id, profile); id++) {
        Serial.print(F("  "));
        Serial.print(id);
        Serial.print(' ');
        Serial.println(profile.name);
    }
}

void setup() {
    // open serial
    Serial.begin(115200);
    printHelp();

    emgCalibrationInit();
    emgCalibrationPrint();
    emgEnvelopeInit();
    emgStatsReset();
}

void loop() {
    EmgEnvelope envelope;

    // printing every sample at 115200 baud takes longer than the 1 ms
    // budget, so only print once per finished group of GROUPING_SIZE samples
    if (emgEnvelopeUpdate(envelope)) {
        if (emgCalibrationRunning()) {
            // calibration owns the stats module while it runs, and the
            // stream is hidden so its prompts and countdown stay readable
            emgCalibrationAdd(envelope);
            return;
        } else if (warmupRemaining > 0) {
            warmupRemaining--;
        } else {
            emgStatsAdd(envelope);
        }

        Serial.print(">power:");
        Serial.println(envelope.power);
        Serial.print(">activation:");
        Serial.println(emgCalibrationActivation(envelope.power));
        if (TIMING_DEBUG) {
            // unfiltered value, useful for tuning the artifact limit
            
            // Serial.print(">rawPower:");
            // Serial.println(envelope.rawPower);
            
            // worst filter cost in the group; must stay under 1 ms
            // (the filter cost average around 520 us)
            
            // Serial.print(">costUs:");
            // Serial.println(envelope.maxCostUs);
        }
    }

    // serial commands, e.g. reset before moving the electrodes to a new
    // body part, then print once that recording is done
    if (Serial.available() > 0) {
        char command = Serial.read();
        if (command == 'c') {
            if (!emgCalibrationStart()) {
                Serial.println(F("Calibration already running."));
            }
        } else if (emgCalibrationRunning()) {
            // 's' / 'r' would disturb the stats calibration is collecting
            if (command != '\n' && command != '\r') {
                Serial.println(F("Busy calibrating."));
            }
        } else if (command == 's') {
            emgStatsPrint();
        } else if (command == 'r') {
            emgStatsReset();
            Serial.println(F("Statistics reset."));
        } else if (command == 'i') {
            emgCalibrationPrint();
        } else if (command == 'x') {
            emgCalibrationErase();
            Serial.println(F("Calibration erased; using defaults."));
        } else if (command >= '0' && command <= '9') {
            if (emgCalibrationSelectLocation(command - '0')) {
                Serial.print(F("Selected location "));
                Serial.print(command);
                Serial.println(F("; send 'c' to calibrate it."));
            } else {
                Serial.println(F("Unknown location."));
            }
        }
    }
    // if costUs gets close to the 1 ms budget, the sample rate need to
    // reduce to SAMPLE_FREQ_500HZ
}
