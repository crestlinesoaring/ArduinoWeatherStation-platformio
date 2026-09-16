/*
 * One-shot Athena network EEPROM writer.
 * Built via: pio run -e <env>-eeprom-net -t upload
 * Or:        pio run -e tavis -t eeprom-net  (uploads this sketch automatically)
 *
 * Patches Athena network settings (writeNet/writePort + CS/reset pins).
 * Optionally writes weather-station eeHardwareId (byte 75) from HW_VERSION.
 */

#include <avr/eeprom.h>
#include <AthenaEEPROM.h>

#ifndef IP_GW
#error "IP_GW must be defined in build_flags"
#endif
#ifndef IP_Q3
#error "IP_Q3 must be defined in build_flags"
#endif
#ifndef IP_WX
#error "IP_WX must be defined in build_flags"
#endif

#ifndef ATHENA_TFTP_PORT
#define ATHENA_TFTP_PORT 46969
#endif

#ifndef ATHENA_ETH_CS_PIN
#define ATHENA_ETH_CS_PIN 53
#endif

#ifndef ATHENA_ETH_RESET_PIN
#define ATHENA_ETH_RESET_PIN 6
#endif

#ifndef MAC_5
#define MAC_5 0x02
#endif

#ifndef MAC_6
#define MAC_6 IP_WX
#endif

#ifndef HW_VERSION
#error "HW_VERSION must be defined (passed from parent env by eeprom-net)"
#endif

static const uint8_t EE_HARDWARE_ID = 75;
static const uint8_t EE_UNINITIALIZED = 0xFF;

static const uint8_t NET_SIG_1 = 0x55;
static const uint8_t NET_SIG_2 = 0xAA;
static const uint8_t NET_SIG_3 = 0xBB;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, MAC_5, MAC_6};
IPAddress ip(192, 168, IP_Q3, IP_WX);
IPAddress gateway(192, 168, IP_Q3, IP_GW);
IPAddress subnet(255, 255, 255, 0);

static uint8_t eepromByte(uint16_t addr) {
  return eeprom_read_byte((const uint8_t*)(uintptr_t)addr);
}

static IPAddress readAddr(uint16_t start) {
  return IPAddress(eepromByte(start), eepromByte(start + 1), eepromByte(start + 2),
                   eepromByte(start + 3));
}

static void readMac(byte* out) {
  for (uint8_t i = 0; i < 6; i++) {
    out[i] = eepromByte(13 + i);
  }
}

static bool byteWritable(uint8_t cur, uint8_t want) {
  return (cur & want) == want;
}

static bool networkWritable() {
  if (!byteWritable(eepromByte(3), NET_SIG_1) || !byteWritable(eepromByte(4), NET_SIG_2)) {
    return false;
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (!byteWritable(eepromByte(5 + i), gateway[i]) ||
        !byteWritable(eepromByte(9 + i), subnet[i]) ||
        !byteWritable(eepromByte(19 + i), ip[i])) {
      return false;
    }
  }
  for (uint8_t i = 0; i < 6; i++) {
    if (!byteWritable(eepromByte(13 + i), mac[i])) {
      return false;
    }
  }
  if (!byteWritable(eepromByte(23), NET_SIG_3)) {
    return false;
  }
  uint8_t portLo = ATHENA_TFTP_PORT & 0xFF;
  uint8_t portHi = ATHENA_TFTP_PORT >> 8;
  if (!byteWritable(eepromByte(24), portLo) || !byteWritable(eepromByte(25), portHi)) {
    return false;
  }
  if (!byteWritable(eepromByte(68), ATHENA_ETH_CS_PIN) ||
      !byteWritable(eepromByte(69), ATHENA_ETH_RESET_PIN)) {
    return false;
  }
  return true;
}

static bool networkMatches() {
  if (eepromByte(3) != NET_SIG_1 || eepromByte(4) != NET_SIG_2) {
    return false;
  }
  if (readAddr(5) != gateway || readAddr(9) != subnet || readAddr(19) != ip) {
    return false;
  }
  byte storedMac[6];
  readMac(storedMac);
  for (uint8_t i = 0; i < 6; i++) {
    if (storedMac[i] != mac[i]) {
      return false;
    }
  }
  if (eepromByte(23) != NET_SIG_3) {
    return false;
  }
  uint16_t port = eepromByte(24) | (static_cast<uint16_t>(eepromByte(25)) << 8);
  if (port != ATHENA_TFTP_PORT) {
    return false;
  }
  if (eepromByte(68) != ATHENA_ETH_CS_PIN || eepromByte(69) != ATHENA_ETH_RESET_PIN) {
    return false;
  }
  return true;
}

static void printMac(const byte* value) {
  for (byte i = 0; i < 6; i++) {
    if (i) {
      Serial.print(':');
    }
    if (value[i] < 16) {
      Serial.print('0');
    }
    Serial.print(value[i], HEX);
  }
}

static void printVerifyLine() {
  byte storedMac[6];
  readMac(storedMac);
  uint16_t port = eepromByte(24) | (static_cast<uint16_t>(eepromByte(25)) << 8);

  Serial.print(F("[eeprom-net] VERIFY GW="));
  Serial.print(readAddr(5));
  Serial.print(F(" IP="));
  Serial.print(readAddr(19));
  Serial.print(F(" SN="));
  Serial.print(readAddr(9));
  Serial.print(F(" MAC="));
  printMac(storedMac);
  Serial.print(F(" PORT="));
  Serial.print(port);
  Serial.print(F(" CS="));
  Serial.print(eepromByte(68));
  Serial.print(F(" RESET="));
  Serial.print(eepromByte(69));
  Serial.println(networkMatches() ? F(" OK") : F(" FAIL"));

  Serial.print(F("[eeprom-net] RAW "));
  for (uint8_t addr = 5; addr <= 25; addr++) {
    if (addr != 5) {
      Serial.print(' ');
    }
    Serial.print(addr);
    Serial.print('=');
    uint8_t v = eepromByte(addr);
    if (v < 16) {
      Serial.print('0');
    }
    Serial.print(v, HEX);
  }
  Serial.println();
}

static void writeHardwareId(uint8_t id) {
  eeprom_write_byte((uint8_t*)(uintptr_t)EE_HARDWARE_ID, 0);
  eeprom_write_byte((uint8_t*)(uintptr_t)EE_HARDWARE_ID, id);
}

static void writeHardwareIdIfNeeded() {
  const uint8_t stored = eepromByte(EE_HARDWARE_ID);
  const uint8_t target = HW_VERSION;

#ifdef EEPROM_NET_FORCE_HW_ID
  Serial.print(F("[eeprom-net] force-writing hwId="));
  Serial.println(target);
  writeHardwareId(target);
#elif stored == EE_UNINITIALIZED
  Serial.print(F("[eeprom-net] writing hwId="));
  Serial.println(target);
  writeHardwareId(target);
#else
  Serial.print(F("[eeprom-net] hwId="));
  Serial.print(stored);
  Serial.println(F(" (unchanged)"));
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);

  writeHardwareIdIfNeeded();

  if (networkMatches()) {
    Serial.println(F("[eeprom-net] network settings already match; skipping write"));
    EEPROM.writeImgOk();
    printVerifyLine();
    return;
  }

  if (!networkWritable()) {
    Serial.println(F("[eeprom-net] EEPROM cells cannot be updated in place (need chip erase)"));
    printVerifyLine();
    return;
  }

  Serial.println(F("[eeprom-net] writing Athena network settings..."));
  Serial.print(F("  target IP="));
  Serial.print(ip);
  Serial.print(F(" GW="));
  Serial.print(gateway);
  Serial.print(F(" SN="));
  Serial.println(subnet);
  Serial.print(F("  target MAC="));
  printMac(mac);
  Serial.println();

  EEPROM.writeNet(mac, ip, gateway, subnet);
  EEPROM.writePort(ATHENA_TFTP_PORT);
  EEPROM.setEthernetCSPin(ATHENA_ETH_CS_PIN);
  EEPROM.setEthernetResetPin(ATHENA_ETH_RESET_PIN);
  EEPROM.writeImgOk();

  printVerifyLine();
  Serial.println(F("[eeprom-net] done (re-flash main firmware when finished)"));
}

void loop() {
  delay(1000);
}
