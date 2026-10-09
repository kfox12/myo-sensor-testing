#include <Arduino.h>

#include "emg_envelope.h"

#define TIMING_DEBUG 1

void setup() {
    emgEnvelopeInit();

    // open serial
    Serial.begin(115200);
}

void loop() {
    EmgEnvelope envelope;

    // printing every sample at 115200 baud takes longer than the 1 ms
    // budget, so only print once per finished group of GROUPING_SIZE samples
    if (emgEnvelopeUpdate(envelope)) {
        Serial.print(">power:");
        Serial.println(envelope.power);
        if (TIMING_DEBUG) {
            // unfiltered value, useful for tuning ARTIFACT_LIMIT
            Serial.print(">rawPower:");
            Serial.println(envelope.rawPower);
            // worst filter cost in the group; must stay under 1 ms
            // (the filter cost average around 520 us)
            Serial.print(">costUs:");
            Serial.println(envelope.maxCostUs);
        }
    }
    // if costUs gets close to the 1 ms budget, the sample rate need to
    // reduce to SAMPLE_FREQ_500HZ
}
