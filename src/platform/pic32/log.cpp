#include "util/log.h"
#include <stdio.h>
#include <stdarg.h>
#include "WProgram.h"

extern HardwareSerial Serial2;

extern HardwareSerial Serial;

void openxc::util::log::initialize() {	


#if defined(CROSSCHASM_BTLE_C5)
		Serial.begin(115200);
#else 
		Serial2.begin(115200);
#endif
}

void openxc::util::log::debugUart(const char* message) {


#if defined(CROSSCHASM_BTLE_C5)
		Serial.print(message);
#else 
		Serial2.print(message);
#endif
}
