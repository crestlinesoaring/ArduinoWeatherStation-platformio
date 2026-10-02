#include "renogy.h"
#include <ModbusMaster.h>

// Renogy Wanderer RS232/Modbus on Mega Serial2 (9600 baud, slave address 255).
static ModbusMaster renogyNode;
static const uint8_t RENOGY_MODBUS_ADDRESS = 255;
static const uint32_t RENOGY_DATA_REGISTER_COUNT = 9; // through solar amps at index 8

float renogy_solar_volts = 0;
float renogy_solar_amps = 0;
bool renogy_connected = false;

void renogyInit() {
  Serial2.begin(9600);
  renogyNode.begin(RENOGY_MODBUS_ADDRESS, Serial2);
}

void renogyUpdate() {
  uint8_t result = renogyNode.readHoldingRegisters(0x100, RENOGY_DATA_REGISTER_COUNT);
  if (result == renogyNode.ku8MBSuccess) {
    renogy_connected = true;
    renogy_solar_volts = renogyNode.getResponseBuffer(7) * 0.1f;
    renogy_solar_amps = renogyNode.getResponseBuffer(8) * 0.01f;
  } else {
    renogy_connected = false;
    renogy_solar_volts = 0;
    renogy_solar_amps = 0;
  }
}
