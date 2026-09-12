#include "wxSerial.h"

void wxLogBlank() {
  Serial.println();
}

void wxLogRule() {
  Serial.println(F("----------------------------------------"));
}

void wxLogSection(const __FlashStringHelper* title) {
  Serial.println();
  Serial.println(title);
  wxLogRule();
}

void wxLogTag(const __FlashStringHelper* tag, const __FlashStringHelper* msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTag(const __FlashStringHelper* tag, const String& msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTag(const __FlashStringHelper* tag, const char* msg) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.println(msg);
}

void wxLogTagNum(const __FlashStringHelper* tag, const __FlashStringHelper* label, long value) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.print(label);
  Serial.println(value);
}

void wxLogTagFloat(const __FlashStringHelper* tag, const __FlashStringHelper* label, float value, uint8_t decimals) {
  Serial.print(F("["));
  Serial.print(tag);
  Serial.print(F("] "));
  Serial.print(label);
  Serial.println(value, decimals);
}
