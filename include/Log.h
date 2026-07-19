#pragma once

#include <Arduino.h>

namespace Log {

template <typename... Args>
inline void info(const char* format, Args... args) {
  Serial.printf("[INFO] ");
  Serial.printf(format, args...);
  Serial.println();
}

template <typename... Args>
inline void warning(const char* format, Args... args) {
  Serial.printf("[WARNING] ");
  Serial.printf(format, args...);
  Serial.println();
}

template <typename... Args>
inline void error(const char* format, Args... args) {
  Serial.printf("[ERROR] ");
  Serial.printf(format, args...);
  Serial.println();
}

template <typename... Args>
inline void debug(const char* format, Args... args) {
  Serial.printf("[DEBUG] ");
  Serial.printf(format, args...);
  Serial.println();
}

}  // namespace Log

