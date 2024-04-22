#pragma once
#ifndef ARDUINO
#include <stdlib.h>
#include <string.h>

#ifndef min
# define min(a,b) a<b?a:b
#endif

#ifndef may
# define max(a,b) a>b?a:b
#endif

#ifndef HIGH
# define HIGH 1
#endif

#ifndef LOW
# define LOW 0
#endif

#ifndef INPUT
# define INPUT 0x01
# define OUTPUT 0x02
# define INPUT_PULLUP 0x05
#endif

#ifndef PROGMEM
#  define PROGMEM
#endif

namespace arduino_vs1053 {

// If you want to use the project outside of Arduino you need to implement the following 
// methods:

void delay(int);
void yield();
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
void pinMode(uint8_t, uint8_t);
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

}

#endif