#pragma once

// declare namespace
namespace arduino_vs1053{}

// Activate/Deactivate Logging in Arduino: Uncomment out define to activate
// #define DEBUG_PORT Serial


// I2S Configuration: Use custom SPI Class for ESP
#define USE_ESP_SPI_CUSTOM 0

// In Arduino we usually do not want to work with namespaces. So we bring in the relevant namespace automatically.
// Comment out or define VS1053_NO_USING_NAMESPACE if you need to use namespace in sketch
#ifndef VS1053_NO_USING_NAMESPACE
using namespace arduino_vs1053;
#endif
