#include <Arduino.h>
#include <limits.h>

#include "emg_calibration.h"
#include "emg_stats.h"

#define GROUP_MS 10 // GROUPING_SIZE samples at 1 kHz
#define GROUPS_PER_SECOND (1000 / GROUP_MS)

#define SETTLE_MS 2000
#define REST_MS 8000
#define HOLD_MS 3000
#define RELAX_MS 3000
// skip the start of each hold while the user reacts and ramps up, so the
// ramp doesn't drag the max-contraction mean down
#define HOLD_LEAD_IN_MS 500
#define HOLD_COUNT 2

// reject the run if more than this share of recorded groups was zeroed by
// artifact rejection
#define MAX_SUPPRESSED_PERCENT 10

enum Phase : uint8_t {
    PHASE_IDLE,
    PHASE_SETTLE,
    PHASE_REST,
    PHASE_HOLD_1,
    PHASE_RELAX,
    PHASE_HOLD_2,
};

static Phase phase = PHASE_IDLE;
static unsigned int phaseGroups;    // groups seen in the current phase
static uint8_t selectedLocation = LOCATION_GENERIC;
static UserCalibration current;     // active calibration (saved or defaults)

// accumulated over the run
static long restP99;
static float holdMeanSum;
static unsigned long recordedGroups;
static unsigned long clippedGroups;
static unsigned long suppressedGroups;

static void setDefaults(UserCalibration &cal, uint8_t locationId) {
    cal.locationId = locationId;
    cal.noiseThreshold = 0;
    cal.mvcPower = 0;
    cal.onThreshold = 0;
    cal.offThreshold = 0;
    cal.artifactLimit = ARTIFACT_LIMIT;
}

static void printLocation(uint8_t id) {
    LocationProfile profile;
    Serial.print(id);
    if (emgProfileGet(id, profile)) {
        Serial.print(' ');
        Serial.println(profile.name);
    } else {
        Serial.println(F(" (unknown)"));
    }
}

static unsigned int phaseDurationGroups(Phase p) {
    switch (p) {
    case PHASE_SETTLE: return SETTLE_MS / GROUP_MS;
    case PHASE_REST:   return REST_MS / GROUP_MS;
    case PHASE_HOLD_1:
    case PHASE_HOLD_2: return HOLD_MS / GROUP_MS;
    case PHASE_RELAX:  return RELAX_MS / GROUP_MS;
    default:           return 0;
    }
}

// Groups at the start of a phase that are not recorded; UINT_MAX = none are.
static unsigned int phaseLeadInGroups(Phase p) {
    switch (p) {
    case PHASE_REST:   return 0;
    case PHASE_HOLD_1:
    case PHASE_HOLD_2: return HOLD_LEAD_IN_MS / GROUP_MS;
    default:           return UINT_MAX;
    }
}

static void startPhase(Phase p) {
    phase = p;
    phaseGroups = 0;
    emgStatsReset();

    // keep prompts short: the serial buffer is 64 bytes and a longer line
    // would stall sampling while it drains
    switch (p) {
    case PHASE_SETTLE: Serial.println(F("Keep still...")); break;
    case PHASE_REST:   Serial.println(F("Relax completely (8 s)")); break;
    case PHASE_HOLD_1: Serial.println(F("Squeeze as hard as comfortable, hold (3 s)")); break;
    case PHASE_RELAX:  Serial.println(F("Relax (3 s)")); break;
    case PHASE_HOLD_2: Serial.println(F("Squeeze and hold again (3 s)")); break;
    default: break;
    }
}

static void finish() {
    phase = PHASE_IDLE;
    long mvcPower = lround(holdMeanSum / HOLD_COUNT);

    LocationProfile profile;
    emgProfileGet(selectedLocation, profile);
    CalibrationResult result = emgCalibrationValidate(
        profile, restP99, mvcPower, recordedGroups, clippedGroups, suppressedGroups);

    Serial.print(F("rest p99: "));
    Serial.print(restP99);
    Serial.print(F(", max contraction: "));
    Serial.print(mvcPower);
    Serial.print(F(", SNR: "));
    Serial.println((float)mvcPower / (restP99 > 0 ? restP99 : 1), 1);

    if (result == CAL_OK) {
        emgCalibrationCompute(profile, selectedLocation, restP99, mvcPower, current);
        emgStorageSave(current);
        Serial.println(F("Calibration saved."));
        emgCalibrationPrint();
    } else {
        Serial.print(F("Calibration failed: "));
        switch (result) {
        case CAL_CLIPPED:
            Serial.println(F("signal clipped, lower sensor gain"));
            break;
        case CAL_TOO_MANY_ARTIFACTS:
            Serial.println(F("too many artifacts, hold still / tape cables, retry"));
            break;
        case CAL_LOW_SNR:
            Serial.println(F("signal too weak vs rest, reposition electrodes"));
            break;
        default:
            break;
        }
        Serial.println(F("Keeping previous calibration."));
    }

    // hand the stats module back to manual 's' / 'r' use with a clean slate
    emgStatsReset();
}

static void endPhase() {
    if (phaseLeadInGroups(phase) != UINT_MAX) {
        EmgStatsSummary summary = emgStatsGetSummary();
        recordedGroups += summary.totalGroups;
        clippedGroups += summary.clippedGroups;
        suppressedGroups += summary.suppressedGroups;
        if (phase == PHASE_REST) {
            restP99 = summary.p99;
        } else {
            holdMeanSum += summary.meanPower;
        }
    }

    switch (phase) {
    case PHASE_SETTLE: startPhase(PHASE_REST); break;
    case PHASE_REST:   startPhase(PHASE_HOLD_1); break;
    case PHASE_HOLD_1: startPhase(PHASE_RELAX); break;
    case PHASE_RELAX:  startPhase(PHASE_HOLD_2); break;
    case PHASE_HOLD_2: finish(); break;
    default: break;
    }
}

void emgCalibrationInit() {
    if (emgStorageLoad(current) && current.locationId < emgProfileCount()) {
        selectedLocation = current.locationId;
    } else {
        setDefaults(current, LOCATION_GENERIC);
        selectedLocation = LOCATION_GENERIC;
    }
}

bool emgCalibrationStart() {
    if (phase != PHASE_IDLE) {
        return false;
    }
    restP99 = 0;
    holdMeanSum = 0;
    recordedGroups = 0;
    clippedGroups = 0;
    suppressedGroups = 0;

    Serial.print(F("Calibrating location "));
    printLocation(selectedLocation);
    startPhase(PHASE_SETTLE);
    return true;
}

bool emgCalibrationRunning() {
    return phase != PHASE_IDLE;
}

void emgCalibrationAdd(const EmgEnvelope &group) {
    if (phase == PHASE_IDLE) {
        return;
    }
    phaseGroups++;
    if (phaseGroups > phaseLeadInGroups(phase)) {
        emgStatsAdd(group);
    }
    unsigned int remainingGroups = phaseDurationGroups(phase) - phaseGroups;
    if (remainingGroups == 0) {
        endPhase();
    } else if (remainingGroups % GROUPS_PER_SECOND == 0) {
        // countdown: seconds left in this phase
        Serial.print(F("  "));
        Serial.println(remainingGroups / GROUPS_PER_SECOND);
    }
}

bool emgCalibrationSelectLocation(uint8_t id) {
    if (phase != PHASE_IDLE || id >= emgProfileCount()) {
        return false;
    }
    selectedLocation = id;
    return true;
}

void emgCalibrationPrint() {
    Serial.println(F("----- Calibration -----"));
    Serial.print(F("location:        "));
    printLocation(current.locationId);
    Serial.print(F("status:          "));
    Serial.println(current.mvcPower > 0 ? F("calibrated") : F("not calibrated (defaults)"));
    Serial.print(F("noise threshold: "));
    Serial.println(current.noiseThreshold);
    Serial.print(F("max contraction: "));
    Serial.println(current.mvcPower);
    Serial.print(F("on threshold:    "));
    Serial.println(current.onThreshold);
    Serial.print(F("off threshold:   "));
    Serial.println(current.offThreshold);
    Serial.print(F("artifact limit:  "));
    Serial.println(current.artifactLimit);
    if (selectedLocation != current.locationId) {
        Serial.print(F("next 'c' uses:   "));
        printLocation(selectedLocation);
    }
    Serial.println(F("-----------------------"));
}

bool emgCalibrationErase() {
    if (phase != PHASE_IDLE) {
        return false;
    }
    emgStorageErase();
    setDefaults(current, selectedLocation);
    return true;
}

int emgCalibrationActivation(long power) {
    if (current.mvcPower <= 0) {
        return 0;
    }
    // float avoids overflowing power * 100
    float percent = (float)power * 100.0f / current.mvcPower;
    return (percent > 100.0f) ? 100 : (int)percent;
}

CalibrationResult emgCalibrationValidate(const LocationProfile &profile,
                                         long restP99, long mvcPower,
                                         unsigned long recordedGroups,
                                         unsigned long clippedGroups,
                                         unsigned long suppressedGroups) {
    if (clippedGroups > 0) {
        return CAL_CLIPPED;
    }
    if (recordedGroups == 0 ||
        suppressedGroups * 100 > recordedGroups * MAX_SUPPRESSED_PERCENT) {
        return CAL_TOO_MANY_ARTIFACTS;
    }
    // a perfectly quiet rest (p99 = 0) still needs a nonzero floor
    long restFloor = (restP99 > 0) ? restP99 : 1;
    if ((float)mvcPower < profile.minSnr * restFloor) {
        return CAL_LOW_SNR;
    }
    return CAL_OK;
}

void emgCalibrationCompute(const LocationProfile &profile, uint8_t locationId,
                           long restP99, long mvcPower, UserCalibration &out) {
    out.locationId = locationId;
    out.noiseThreshold = lround(profile.noiseFactor * restP99);
    out.mvcPower = mvcPower;
    out.onThreshold = lround(profile.onFraction * mvcPower);
    out.offThreshold = lround(profile.offFraction * mvcPower);
    out.artifactLimit = lround(profile.artifactFactor * mvcPower);
}
