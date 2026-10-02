#ifndef RENOGY_H
#define RENOGY_H

#include <Arduino.h>

extern float renogy_solar_volts;
extern float renogy_solar_amps;
extern bool renogy_connected;

void renogyInit();
void renogyPoll();
void renogyFinalizeMinute();

#endif
