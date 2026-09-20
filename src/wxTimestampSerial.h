#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TimeLib.h>

// Wraps Serial0 and prepends a timestamp at the start of each output line.
class WxTimestampSerial : public Stream {
  HardwareSerial& _out;
  bool _lineStart = true;

  void writeTimestamp() {
    _out.print('[');
    if (timeStatus() != timeNotSet) {
      const time_t t = now();
      if (hour(t) < 10) {
        _out.print('0');
      }
      _out.print(hour(t));
      _out.print(':');
      if (minute(t) < 10) {
        _out.print('0');
      }
      _out.print(minute(t));
      _out.print(':');
      if (second(t) < 10) {
        _out.print('0');
      }
      _out.print(second(t));
    } else {
      _out.print(millis());
      _out.print(F("ms"));
    }
    _out.print(F("] "));
  }

 public:
  explicit WxTimestampSerial(HardwareSerial& out) : _out(out) {}

  void begin(unsigned long baud) { _out.begin(baud); }
  void end() { _out.end(); }

  int available() override { return _out.available(); }
  int read() override { return _out.read(); }
  int peek() override { return _out.peek(); }
  void flush() override { _out.flush(); }

  size_t write(uint8_t c) override {
    if (_lineStart && c != '\r' && c != '\n') {
      writeTimestamp();
      _lineStart = false;
    }
    if (c == '\n') {
      _lineStart = true;
    }
    return _out.write(c);
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t n = 0;
    while (size--) {
      if (write(*buffer++)) {
        n++;
      } else {
        break;
      }
    }
    return n;
  }
};

extern WxTimestampSerial WxSerial;  // defined in wxTimestampSerial.cpp
