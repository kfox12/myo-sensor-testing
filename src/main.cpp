#include <Arduino.h>

#include "emg_envelope.h"
#include "emg_stats.h"

#define TIMING_DEBUG 1

// skip the filters' startup transient (10 ms per group) before collecting
// statistics, so it doesn't show up as a huge max
#define STATS_WARMUP_GROUPS 50

static int warmupRemaining = STATS_WARMUP_GROUPS;

void setup() {
    emgEnvelopeInit();
    emgStatsReset();

    // open serial
    Serial.begin(115200);
    Serial.println(F("Send 's' for a statistics summary, 'r' to reset them."));
}

void loop() {
    EmgEnvelope envelope;

    // printing every sample at 115200 baud takes longer than the 1 ms
    // budget, so only print once per finished group of GROUPING_SIZE samples
    if (emgEnvelopeUpdate(envelope)) {
        if (warmupRemaining > 0) {
            warmupRemaining--;
        } else {
            emgStatsAdd(envelope);
        }

        Serial.print(">power:");
        Serial.println(envelope.power);
        if (TIMING_DEBUG) {
            // unfiltered value, useful for tuning ARTIFACT_LIMIT
            
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
        if (command == 's') {
            emgStatsPrint();
        } else if (command == 'r') {
            emgStatsReset();
            Serial.println(F("Statistics reset."));
        }
    }
    // if costUs gets close to the 1 ms budget, the sample rate need to
    // reduce to SAMPLE_FREQ_500HZ
}
