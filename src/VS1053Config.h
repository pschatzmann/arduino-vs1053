#pragma once

// declare namespace
namespace arduino_vs1053{}

// Define Logging Port
#ifndef VS1053_LOG_PORT
#  define VS1053_LOG_PORT Serial
#endif

// Define the size of the log buffer
#ifndef LOG_BUFFER_SIZE
#  define LOG_BUFFER_SIZE 100
#endif

// Enable support for MIDI: set to 0 to minimize memory usage
#ifndef USE_MIDI
#  define USE_MIDI 1
#endif

// Load patches: set to 0 to minimize memory usage
#ifndef USE_PATCHES
#  define USE_PATCHES 1
#endif

// I2S Configuration: Use custom SPI Class for ESP
#ifndef USE_ESP_SPI_CUSTOM
#  define USE_ESP_SPI_CUSTOM 0
#endif

// In Arduino we usually do not want to work with namespaces. So we bring in the relevant namespace automatically.
// Comment out or define VS1053_NO_USING_NAMESPACE if you need to use namespace in sketch
#ifndef VS1053_NO_USING_NAMESPACE
using namespace arduino_vs1053;
#endif
