#pragma once

#include "emg_envelope.h"

// Whole-run signal statistics for characterizing the EMG signal at a
// given electrode placement (body part).
//
// Power statistics (min / max / mean / std dev / percentiles) use only
// groups that survived artifact rejection, so sensor bumps don't skew
// them. Artifacts, suppressed groups, and ADC clipping are counted
// separately.
//
// Suggested use: reset, record a few seconds fully relaxed, print. The
// rest p99 / max is a good starting noise threshold. Then reset, record
// repeated contractions, print. The active p90 against the rest p50
// shows how well that placement separates signal from noise.

// Snapshot of the run so far. Power fields cover clean groups only and
// are 0 when there are none.
struct EmgStatsSummary {
    unsigned long durationMs;
    unsigned long totalGroups;      // every group seen
    unsigned long cleanGroups;      // groups included in power stats
    unsigned long artifactGroups;   // raw average exceeded ARTIFACT_LIMIT
    unsigned long suppressedGroups; // zeroed by artifact rejection (incl. holdoff)
    unsigned long clippedGroups;    // ADC hit 0 or 1023 at least once
    long minPower;
    long maxPower;
    long maxRawPower;               // includes artifacts
    float meanPower;
    float stdDevPower;
    long p10;                       // percentiles are approximate (+/-17%)
    long p50;
    long p90;
    long p99;
    int adcMin;
    int adcMax;
};

// Clear all statistics and restart the run timer.
void emgStatsReset();

// Add one finished group from emgEnvelopeUpdate().
void emgStatsAdd(const EmgEnvelope &group);

// Summarize the run so far.
EmgStatsSummary emgStatsGetSummary();

// Print a human-readable summary of the run so far over Serial.
// Takes roughly 50 ms at 115200 baud, which delays sampling, so only
// call it on demand, not every loop.
void emgStatsPrint();
