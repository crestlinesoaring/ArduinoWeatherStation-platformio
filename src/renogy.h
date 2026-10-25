#ifndef RENOGY_H
#define RENOGY_H

#include <Arduino.h>

// Set to 1 to poll the charge controller on Serial2 each second (Modbus).
#ifndef RENOGY_POLLING_ENABLED
#define RENOGY_POLLING_ENABLED 0
#endif

extern float renogy_solar_volts;
extern float renogy_solar_amps;
extern bool renogy_connected;

// Returns true if Modbus probe succeeded and polling will run this session.
bool renogyInit();
void renogyPoll();
void renogyFinalizeMinute();

#endif
