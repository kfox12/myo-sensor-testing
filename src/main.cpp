#include <Arduino.h>

#if defined(ARDUINO) && ARDUINO >= 100
#else
#include "WProgram.h"
#endif

#include <EMGFilters.h>

#define TIMING_DEBUG 1

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

EMGFilters myFilter;
// discrete filters must works with fixed sample frequence
// our emg filter only support "SAMPLE_FREQ_500HZ" or "SAMPLE_FREQ_1000HZ"
// other sampleRate inputs will bypass all the EMG_FILTER
int sampleRate = SAMPLE_FREQ_1000HZ;
// For countries where power transmission is at 50 Hz
// For countries where power transmission is at 60 Hz, need to change to
// "NOTCH_FREQ_60HZ"
// our emg filter only support 50Hz and 60Hz input
// other inputs will bypass all the EMG_FILTER
int humFreq = NOTCH_FREQ_60HZ;

// Calibration:
// put on the sensors, and release your muscles;
// wait a few seconds, and select the max value as the throhold;
// any value under throhold will be set to zero
static int Throhold = 0 ;
int count = 0;
unsigned long timeStamp;
unsigned long timeBudget;
unsigned long nextSampleTime;   // micros() deadline for the next sample
long envelopeSum = 0;           // running sum over the current group
unsigned long maxCost = 0;      // worst-case processing time in the group
int holdoffRemaining = 0;       // groups left to suppress after an artifact

void setup() {
    /* add setup code here */
    myFilter.init(sampleRate, humFreq, true, true, true);

    // open serial
    Serial.begin(115200);

    // setup for time cost measure
    // using micros()
    timeBudget = 1e6 / sampleRate;
    // micros will overflow and auto return to zero every 70 minutes;
    // the unsigned subtraction in loop() handles that correctly
    nextSampleTime = micros();
}

void loop() {
    /* add main program code here */
    // In order to make sure the ADC sample frequence on arduino,
    // the time cost should be measured each loop
    /*------------start here-------------------*/
    // wait until it's time for the next sample so the filter sees a
    // steady sampleRate (scheduling off the deadline avoids drift)
    while ((long)(micros() - nextSampleTime) < 0) {
    }
    nextSampleTime += timeBudget;
    timeStamp = micros();

    int Value = analogRead(SensorInputPin);

    // filter processing
    int DataAfterFilter = myFilter.update(Value);

    // Use a wider type: squaring an AVR int can overflow (int is 16-bit).
    long power = (long)DataAfterFilter * DataAfterFilter;
    // any value under throhold will be set to zero
    power = (power > Throhold) ? power : 0;

    timeStamp = micros() - timeStamp;
    if (timeStamp > maxCost) {
        maxCost = timeStamp;
    }

    envelopeSum += power;
    count++;

    // printing every sample at 115200 baud takes longer than the 1 ms
    // budget, so only print the group average every GROUPING_SIZE samples
    if (count >= GROUPING_SIZE) {
        long rawAverage = envelopeSum / GROUPING_SIZE;
        long groupAverage = rawAverage;

        if (rawAverage > ARTIFACT_LIMIT) {
            // sensor bump: drop it and restart the holdoff window
            holdoffRemaining = ARTIFACT_HOLDOFF_GROUPS;
            groupAverage = 0;
        } else if (holdoffRemaining > 0) {
            // still settling after a bump
            holdoffRemaining--;
            groupAverage = 0;
        }

        Serial.print(">power:");
        Serial.println(groupAverage);
        if (TIMING_DEBUG) {
            // unfiltered value, useful for tuning ARTIFACT_LIMIT
            Serial.print(">rawPower:");
            Serial.println(rawAverage);
            // worst filter cost in the group; must stay under timeBudget
            // (the filter cost average around 520 us)
            Serial.print(">costUs:");
            Serial.println(maxCost);
        }
        envelopeSum = 0;
        maxCost = 0;
        count = 0;
    }
    /*------------end here---------------------*/
    // if costUs gets close to timeBudget, the sample rate need to reduce to
    // SAMPLE_FREQ_500HZ
}
