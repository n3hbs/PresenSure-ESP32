#pragma once

#include <Arduino.h>

enum class StatusCode : uint8_t {
  OK = 0,
  INVALID_PAYLOAD,
  INVALID_SIGNATURE,
  SESSION_EXPIRED,
  DEVICE_BUSY,
  ALREADY_ACTIVE,
  STORAGE_ERROR,
  ADVERTISEMENT_START_FAILED,
  NO_ACTIVE_SESSION,
  CLOCK_NOT_SET,
  BLE_INITIALIZATION_FAILURE,
};

/** Values supplied by the instructor app for one attendance session. */
struct SessionConfiguration {
  String command;
  String sessionId;
  String sessionCode;
  String token;
  String attendanceMode = "ble_and_face";
  bool continuousChecking = false;
  uint32_t issuedAt = 0;
  uint32_t expiresAt = 0;
  String signature;
  bool active = false;
};

/** Parses, validates, and serializes session configuration data. */
class AppConfig {
 public:
  /** Parses the instructor JSON document into a configuration object. */
  static StatusCode fromJson(const String& json, SessionConfiguration& output);

  /** Serializes the temporary session state for diagnostics. */
  static String toJson(const SessionConfiguration& config);

  /** Validates field ranges and required values, excluding the signature. */
  static StatusCode validate(const SessionConfiguration& config);

  /** Returns the stable byte representation covered by the HMAC signature. */
  static String canonicalPayload(const SessionConfiguration& config);

  /** Returns a stable text label for a firmware status. */
  static const char* statusText(StatusCode status);
};
