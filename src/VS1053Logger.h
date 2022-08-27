#pragma once
/**
 * Licensed under GNU GPLv3 <http://gplv3.fsf.org/>
 * Copyright © 2018
 *
 * @author Marcin Szalomski (github: @baldram | twitter: @baldram)
 */

/**
 * To enable debug, add build flag to your platformio.ini as below (depending on platform).
 *
 * For ESP8266:
 *      build_flags = -D DEBUG_PORT=Serial
 *
 * For ESP32:
 *      build_flags = -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
 *
 */
#ifdef ARDUINO_ARCH_ESP32
    #include "Arduino.h"
    #define LOG(...) ESP_LOGD("ESP_VS1053", __VA_ARGS__)
#elif defined(DEBUG_PORT) 
    #if (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_RP2040))
        #define LOG(...) DEBUG_PORT.printf(__VA_ARGS__)
    #else 
        #include <stdio.h>
        #define LOG(...) {char msg[80]; snprintf(msg, 80,__VA_ARGS__); Serial.println(msg);}
    #endif
#else 
    #define LOG(...) 
#endif

