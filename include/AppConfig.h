#pragma once

#include <Arduino.h>

enum class StatusCode : uint8_t {
  OK = 0,
  INVALID_PAYLOAD,
  SESSION_EXPIRED,
  DEVICE_BUSY,
  ALREADY_ACTIVE,
  AUTHENTICATED,
  NOT_AUTHENTICATED,
  AUTHENTICATION_FAILED,
  ROOM_MISMATCH,
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
  uint32_t scheduleId = 0;
  String sessionCode;
  String subjectCode;
  String roomCode;
  String token;
  String attendanceMode = "ble_and_face";
  bool continuousChecking = false;
  uint32_t issuedAt = 0;
  uint32_t expiresAt = 0;
  bool active = false;
};

/** Parses, validates, and serializes session configuration data. */
class AppConfig {
 public:
  /** Parses the instructor JSON document into a configuration object. */
  static StatusCode fromJson(const String& json, SessionConfiguration& output);

  /** Serializes the temporary session state for diagnostics. */
  static String toJson(const SessionConfiguration& config);

  /** Validates required session fields and their ranges. */
  static StatusCode validate(const SessionConfiguration& config);

  /** Returns a stable text label for a firmware status. */
  static const char* statusText(StatusCode status);
};
