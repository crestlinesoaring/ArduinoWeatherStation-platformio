#ifdef SERIAL_TIMESTAMPS

#include "wxTimestampSerial.h"

// Compiled without wxSerial.h so Serial is still the hardware UART (USB on Mega).
extern HardwareSerial Serial;

WxTimestampSerial WxSerial(Serial);

#endif
