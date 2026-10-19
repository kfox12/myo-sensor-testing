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

// Clear all statistics and restart the run timer.
void emgStatsReset();

// Add one finished group from emgEnvelopeUpdate().
void emgStatsAdd(const EmgEnvelope &group);

// Print a human-readable summary of the run so far over Serial.
// Takes roughly 50 ms at 115200 baud, which delays sampling, so only
// call it on demand, not every loop.
void emgStatsPrint();
