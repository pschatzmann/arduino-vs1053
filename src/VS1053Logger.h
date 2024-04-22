#pragma once

#pragma once
#include "Arduino.h"
#include "VS1053Config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace arduino_vs1053 {

enum VS1053LogLevel_t { VS1053Debug, VS1053Info, VS1053Warning, VS1053Error };

/**
 * @brief Logging class which supports multiple log levels
 * 
 */
class VS1053LoggerClass {
public:
  // global actual loggin level for application
  VS1053LogLevel_t logLevel = VS1053Warning;

  /// start the logger
  bool begin(Print &out, VS1053LogLevel_t l) {
    p_out = &out;
    logLevel = l;
    return true;
  }

  /// Print log message
  void log(VS1053LogLevel_t level, const char *fmt...) {
    if (logLevel <= level) { // AUDIOKIT_LOG_LEVEL = Debug
      char log_buffer[LOG_BUFFER_SIZE];
      strcpy(log_buffer,"VS1053 - ");  
      strcat(log_buffer, VS1053_log_msg[level]);
      strcat(log_buffer, ":     ");
      va_list arg;
      va_start(arg, fmt);
      vsnprintf(log_buffer + 9, LOG_BUFFER_SIZE-9, fmt, arg);
      va_end(arg);
      p_out->println(log_buffer);
    }
  }

protected:
  // Error level as string
  const char *VS1053_log_msg[4] = {"Debug", "Info", "Warning", "Error"};
  Print *p_out = &VS1053_LOG_PORT;
};

extern VS1053LoggerClass VS1053Logger;

#define VS1053_LOGD(fmt, ...) VS1053Logger.log(VS1053Debug, fmt, ##__VA_ARGS__)
#define VS1053_LOGI(fmt, ...) VS1053Logger.log(VS1053Info, fmt, ##__VA_ARGS__)
#define VS1053_LOGW(fmt, ...) VS1053Logger.log(VS1053Warning, fmt, ##__VA_ARGS__)
#define VS1053_LOGE(fmt, ...) VS1053Logger.log(VS1053Error, fmt, ##__VA_ARGS__)

} // namespace arduino_vs1053