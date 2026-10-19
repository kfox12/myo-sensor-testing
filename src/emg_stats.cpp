#include <Arduino.h>
#include <limits.h>

#include "emg_stats.h"

// Percentiles come from a log-scale histogram: two bins per power of two,
// so each bin spans about +/-17% around its midpoint. That's coarse but
// fits in 160 bytes of RAM, where storing every value would not.
#define HISTOGRAM_BINS 40
#define ADC_MAX_READING 1023

static unsigned long startMillis;
static unsigned long totalGroups;      // every group seen
static unsigned long cleanGroups;      // groups included in power stats
static unsigned long artifactGroups;   // raw average exceeded ARTIFACT_LIMIT
static unsigned long suppressedGroups; // zeroed by artifact rejection (incl. holdoff)
static unsigned long clippedGroups;    // ADC hit 0 or 1023 at least once
static long minPower;
static long maxPower;
static long maxRawPower;               // includes artifacts; for tuning ARTIFACT_LIMIT
static int adcMin;
static int adcMax;
// 64-bit sums keep the mean exact over long runs (32-bit would overflow
// the sum of squares within seconds)
static uint64_t powerSum;
static uint64_t powerSumSq;
static unsigned long histogram[HISTOGRAM_BINS];

// Bin 0 holds 0, bin 1 holds 1; after that each power of two [2^n, 2^(n+1))
// splits into a lower and upper half.
static int binFor(long value) {
    if (value <= 0) {
        return 0;
    }
    if (value == 1) {
        return 1;
    }
    int bits = 0;
    for (unsigned long v = value; v != 0; v >>= 1) {
        bits++;
    }
    int bin = 2 * (bits - 1) + ((value >> (bits - 2)) & 1);
    return (bin < HISTOGRAM_BINS) ? bin : HISTOGRAM_BINS - 1;
}

static long binLowerEdge(int bin) {
    if (bin < 2) {
        return bin;
    }
    int bits = bin / 2 + 1;
    return (1L << (bits - 1)) + (bin % 2) * (1L << (bits - 2));
}

// Approximate value below which `percent` of clean groups fall.
static long percentile(int percent) {
    unsigned long target = (cleanGroups * percent + 99) / 100;
    if (target == 0) {
        target = 1;
    }
    unsigned long cumulative = 0;
    for (int bin = 0; bin < HISTOGRAM_BINS; bin++) {
        cumulative += histogram[bin];
        if (cumulative >= target) {
            long lower = binLowerEdge(bin);
            if (bin == HISTOGRAM_BINS - 1) {
                return lower;
            }
            long upper = binLowerEdge(bin + 1) - 1;
            return (lower + upper) / 2;
        }
    }
    return 0;
}

void emgStatsReset() {
    startMillis = millis();
    totalGroups = 0;
    cleanGroups = 0;
    artifactGroups = 0;
    suppressedGroups = 0;
    clippedGroups = 0;
    minPower = LONG_MAX;
    maxPower = 0;
    maxRawPower = 0;
    adcMin = ADC_MAX_READING;
    adcMax = 0;
    powerSum = 0;
    powerSumSq = 0;
    for (int bin = 0; bin < HISTOGRAM_BINS; bin++) {
        histogram[bin] = 0;
    }
}

void emgStatsAdd(const EmgEnvelope &group) {
    totalGroups++;

    if (group.rawPower > maxRawPower) {
        maxRawPower = group.rawPower;
    }
    if (group.rawPower > ARTIFACT_LIMIT) {
        artifactGroups++;
    }
    if (group.adcMin < adcMin) {
        adcMin = group.adcMin;
    }
    if (group.adcMax > adcMax) {
        adcMax = group.adcMax;
    }
    // a clipped ADC means the sensor gain is too high for this placement
    if (group.adcMin <= 0 || group.adcMax >= ADC_MAX_READING) {
        clippedGroups++;
    }

    if (group.suppressed) {
        suppressedGroups++;
        return;
    }

    cleanGroups++;
    if (group.power < minPower) {
        minPower = group.power;
    }
    if (group.power > maxPower) {
        maxPower = group.power;
    }
    powerSum += (uint64_t)group.power;
    powerSumSq += (uint64_t)group.power * (uint64_t)group.power;
    histogram[binFor(group.power)]++;
}

void emgStatsPrint() {
    Serial.println(F("----- EMG run statistics -----"));
    Serial.print(F("duration s:        "));
    Serial.println((millis() - startMillis) / 1000.0, 1);
    Serial.print(F("groups total:      "));
    Serial.println(totalGroups);
    Serial.print(F("groups clean:      "));
    Serial.println(cleanGroups);

    if (cleanGroups > 0) {
        float mean = (float)powerSum / cleanGroups;
        float variance = (float)powerSumSq / cleanGroups - mean * mean;
        float stdDev = (variance > 0) ? sqrt(variance) : 0;
        long p10 = percentile(10);
        long p50 = percentile(50);
        long p90 = percentile(90);
        long p99 = percentile(99);

        Serial.print(F("power min:         "));
        Serial.println(minPower);
        Serial.print(F("power max:         "));
        Serial.println(maxPower);
        Serial.print(F("power mean:        "));
        Serial.println(mean, 1);
        Serial.print(F("power std dev:     "));
        Serial.println(stdDev, 1);
        Serial.print(F("power p10 (~):     "));
        Serial.println(p10);
        Serial.print(F("power p50 (~):     "));
        Serial.println(p50);
        Serial.print(F("power p90 (~):     "));
        Serial.println(p90);
        Serial.print(F("power p99 (~):     "));
        Serial.println(p99);
        // contrast between strong activity and the quiet floor
        Serial.print(F("p90/p10 ratio:     "));
        if (p10 > 0) {
            Serial.println((float)p90 / p10, 1);
        } else {
            Serial.println(F("n/a (p10 is 0)"));
        }
    }

    Serial.print(F("raw power max:     "));
    Serial.println(maxRawPower);
    Serial.print(F("artifact groups:   "));
    Serial.println(artifactGroups);
    Serial.print(F("suppressed groups: "));
    Serial.println(suppressedGroups);
    Serial.print(F("ADC min / max:     "));
    Serial.print(adcMin);
    Serial.print(F(" / "));
    Serial.println(adcMax);
    Serial.print(F("clipped groups:    "));
    Serial.println(clippedGroups);
    Serial.println(F("------------------------------"));
}
