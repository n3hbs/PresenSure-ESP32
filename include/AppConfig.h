#pragma once

#include <Arduino.h>

enum class AttendanceType : uint8_t { BLE = 1, FACE = 2, BOTH = 3 };

enum class StatusCode : uint8_t {
  OK = 0,
  INVALID_CONFIGURATION,
  EXPIRED_SESSION,
  INVALID_SIGNATURE,
  MISSING_PARAMETERS,
  STORAGE_FAILURE,
  BLE_INITIALIZATION_FAILURE,
  NO_ACTIVE_SESSION,
  CLOCK_NOT_SET,
};

/** Values supplied by the instructor app for one attendance session. */
struct SessionConfiguration {
  String sessionId;
  AttendanceType attendanceType = AttendanceType::BLE;
  uint32_t startTime = 0;
  uint32_t endTime = 0;
  bool continuousChecking = false;
  String rotatingSecret;
  String signature;
  uint16_t advertisementIntervalMs = 0;
};

/** Parses, validates, and serializes session configuration data. */
class AppConfig {
 public:
  /** Parses the instructor JSON document into a configuration object. */
  static StatusCode fromJson(const String& json, SessionConfiguration& output);

  /** Serializes configuration for NVS persistence. */
  static String toJson(const SessionConfiguration& config);

  /** Validates field ranges and required values, excluding the signature. */
  static StatusCode validate(const SessionConfiguration& config);

  /** Returns the stable byte representation covered by the HMAC signature. */
  static String canonicalPayload(const SessionConfiguration& config);

  /** Returns a stable text label for a firmware status. */
  static const char* statusText(StatusCode status);
};

