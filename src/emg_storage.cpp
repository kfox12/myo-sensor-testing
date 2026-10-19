#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>

#include "emg_storage.h"

#define CALIBRATION_EEPROM_ADDRESS 0

// Fletcher-16: catches the byte swaps and partial writes a plain sum would miss.
uint16_t emgStorageChecksum(const UserCalibration &cal) {
    const uint8_t *bytes = (const uint8_t *)&cal;
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    for (size_t i = 0; i < offsetof(UserCalibration, checksum); i++) {
        sum1 = (sum1 + bytes[i]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    return (sum2 << 8) | sum1;
}

bool emgStorageLoad(UserCalibration &out) {
    EEPROM.get(CALIBRATION_EEPROM_ADDRESS, out);
    return out.magic == CALIBRATION_MAGIC &&
           out.version == CALIBRATION_VERSION &&
           out.checksum == emgStorageChecksum(out);
}

void emgStorageSave(UserCalibration &cal) {
    cal.magic = CALIBRATION_MAGIC;
    cal.version = CALIBRATION_VERSION;
    cal.checksum = emgStorageChecksum(cal);
    // put() only rewrites bytes that changed, sparing EEPROM wear
    EEPROM.put(CALIBRATION_EEPROM_ADDRESS, cal);
}

void emgStorageErase() {
    // 0xFF is the blank-EEPROM value; one byte write per cell
    for (size_t i = 0; i < sizeof(UserCalibration); i++) {
        EEPROM.update(CALIBRATION_EEPROM_ADDRESS + i, 0xFF);
    }
}
