#pragma once
#ifndef ARDUINO

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

// If you want to use the project outside of Arduino you need to implement the following 
// methods:

void delay(int);
void yield();
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
void pinMode(uint8_t, uint8_t);


#endif