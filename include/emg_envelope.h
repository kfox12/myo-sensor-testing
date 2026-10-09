#pragma once

// EMG envelope extraction:
// samples the sensor at a fixed rate, runs each sample through the
// EMGFilters chain (notch, lowpass, highpass), squares it into power,
// averages GROUPING_SIZE samples into one envelope value, and rejects
// motion artifacts (sensor bumps).

#define SensorInputPin A0 // input pin number

#define GROUPING_SIZE 10

// Motion-artifact rejection:
// real muscle activity averages ~3,000-15,000, while the sensor bumping
// the body produces spikes up to ~94,000. Any group average above
// ARTIFACT_LIMIT is treated as a bump and reported as 0, and the output
// stays at 0 for ARTIFACT_HOLDOFF_GROUPS more groups (10 ms each) so the
// decaying tail of the bump and the filter's ringing are also dropped.
#define ARTIFACT_LIMIT 30000L
#define ARTIFACT_HOLDOFF_GROUPS 20

// One finished group of GROUPING_SIZE samples.
struct EmgEnvelope {
    long power;              // group average after artifact rejection
    long rawPower;           // group average before artifact rejection
    unsigned long maxCostUs; // worst per-sample processing time in the group
};

// Configure the filters and start the sample clock. Call once from setup().
void emgEnvelopeInit();

// Wait for the next sample tick, then read and process one sample.
// Returns true when a group has just finished and `out` has been filled.
bool emgEnvelopeUpdate(EmgEnvelope &out);
