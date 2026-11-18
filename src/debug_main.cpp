#include <Arduino.h>

#include <Servo.h>

#include "emg_envelope.h"

// Debug build: no calibration, stats or serial commands. It prints the
// envelope power (one value per 10 ms group) so the raw sensor signal can be
// inspected in the serial monitor or Teleplot, and drives the servo with a
// fixed threshold: power above MOTOR_THRESHOLD turns the motor, otherwise it
// returns to rest.

#define MOTOR_THRESHOLD 10L
const int SERVO_PIN = 9;
const int REST_ANGLE = 0;
const int ACTIVE_ANGLE = 270;  // same as motorTurn.ino (clamped to 180 by Servo)

static Servo motor;
static bool motorOn = false;

void setup() {
    Serial.begin(115200);
    emgEnvelopeInit();
    motor.attach(SERVO_PIN);
    motor.write(REST_ANGLE);
}

void loop() {
    EmgEnvelope envelope;

    // printing every sample at 115200 baud takes longer than the 1 ms
    // budget, so only print once per finished group
    if (emgEnvelopeUpdate(envelope)) {
        Serial.print(">power:");
        Serial.println(envelope.power);

        bool shouldRun = envelope.power > MOTOR_THRESHOLD;
        if (shouldRun != motorOn) {
            motorOn = shouldRun;
            motor.write(motorOn ? ACTIVE_ANGLE : REST_ANGLE);
        }
        // raw ADC range in this group: 0 or 1023 means the pin touched a
        // rail (that is what makes calibration report "clipped")
        Serial.print(">adcMin:");
        Serial.println(envelope.adcMin);
        Serial.print(">adcMax:");
        Serial.println(envelope.adcMax);
    }
}
