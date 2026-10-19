#include <Arduino.h>
#include <EMGFilters.h>

#include "emg_envelope.h"

static EMGFilters myFilter;
// discrete filters must works with fixed sample frequence
// our emg filter only support "SAMPLE_FREQ_500HZ" or "SAMPLE_FREQ_1000HZ"
// other sampleRate inputs will bypass all the EMG_FILTER
static int sampleRate = SAMPLE_FREQ_1000HZ;
// For countries where power transmission is at 50 Hz
// For countries where power transmission is at 60 Hz, need to change to
// "NOTCH_FREQ_60HZ"
// our emg filter only support 50Hz and 60Hz input
// other inputs will bypass all the EMG_FILTER
static int humFreq = NOTCH_FREQ_60HZ;

// Calibration:
// put on the sensors, and release your muscles;
// wait a few seconds, and select the max value as the threshold;
// any value under threshold will be set to zero
static int threshold = 0;
static int count = 0;
static unsigned long timeBudget;
static unsigned long nextSampleTime;   // micros() deadline for the next sample
static long envelopeSum = 0;           // running sum over the current group
static unsigned long maxCost = 0;      // worst-case processing time in the group
static int holdoffRemaining = 0;       // groups left to suppress after an artifact
static int adcMin = 1023;              // lowest raw ADC reading in the group
static int adcMax = 0;                 // highest raw ADC reading in the group

void emgEnvelopeInit() {
    myFilter.init(sampleRate, humFreq, true, true, true);

    // setup for time cost measure
    // using micros()
    timeBudget = 1e6 / sampleRate;
    // micros will overflow and auto return to zero every 70 minutes;
    // the unsigned subtraction in emgEnvelopeUpdate() handles that correctly
    nextSampleTime = micros();
}

bool emgEnvelopeUpdate(EmgEnvelope &out) {
    // wait until it's time for the next sample so the filter sees a
    // steady sampleRate (scheduling off the deadline avoids drift)
    while ((long)(micros() - nextSampleTime) < 0) {
    }
    nextSampleTime += timeBudget;
    unsigned long timeStamp = micros();

    int Value = analogRead(SensorInputPin);
    if (Value < adcMin) {
        adcMin = Value;
    }
    if (Value > adcMax) {
        adcMax = Value;
    }

    // filter processing
    int DataAfterFilter = myFilter.update(Value);

    // Use a wider type: squaring an AVR int can overflow (int is 16-bit).
    long power = (long)DataAfterFilter * DataAfterFilter;
    // any value under threshold will be set to zero
    power = (power > threshold) ? power : 0;

    timeStamp = micros() - timeStamp;
    if (timeStamp > maxCost) {
        maxCost = timeStamp;
    }

    envelopeSum += power;
    count++;

    if (count < GROUPING_SIZE) {
        return false;
    }

    long rawAverage = envelopeSum / GROUPING_SIZE;
    long groupAverage = rawAverage;
    bool suppressed = false;

    if (rawAverage > ARTIFACT_LIMIT) {
        // sensor bump: drop it and restart the holdoff window
        holdoffRemaining = ARTIFACT_HOLDOFF_GROUPS;
        groupAverage = 0;
        suppressed = true;
    } else if (holdoffRemaining > 0) {
        // still settling after a bump
        holdoffRemaining--;
        groupAverage = 0;
        suppressed = true;
    }

    out.power = groupAverage;
    out.rawPower = rawAverage;
    out.maxCostUs = maxCost;
    out.adcMin = adcMin;
    out.adcMax = adcMax;
    out.suppressed = suppressed;

    envelopeSum = 0;
    maxCost = 0;
    adcMin = 1023;
    adcMax = 0;
    count = 0;
    return true;
}
