#include "AppConfig.h"

#include <ArduinoJson.h>

#include "Constants.h"

StatusCode AppConfig::fromJson(const String& json, SessionConfiguration& output) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, json);
  if (error) {
    return StatusCode::INVALID_PAYLOAD;
  }

  constexpr const char* required[] = {"command", "session_id", "schedule_id", "subject_code",
                                      "room_code", "token", "expires_at"};
  for (const char* key : required) {
    if (document[key].isNull()) {
      return StatusCode::INVALID_PAYLOAD;
    }
  }

  output.command = document["command"].as<String>();
  output.sessionId = document["session_id"].as<String>();
  output.scheduleId = document["schedule_id"].as<uint32_t>();
  output.sessionCode = document["session_code"] | "";
  output.subjectCode = document["subject_code"].as<String>();
  output.roomCode = document["room_code"].as<String>();
  output.token = document["token"].as<String>();
  output.attendanceMode = document["attendance_mode"] | "ble_and_face";
  output.continuousChecking = document["continuous_checking"] | false;
  output.issuedAt = document["issued_at"] | 0U;
  output.expiresAt = document["expires_at"].as<uint32_t>();
  return validate(output);
}

String AppConfig::toJson(const SessionConfiguration& config) {
  JsonDocument document;
  document["command"] = config.command;
  document["session_id"] = config.sessionId;
  document["schedule_id"] = config.scheduleId;
  if (!config.sessionCode.isEmpty()) document["session_code"] = config.sessionCode;
  document["subject_code"] = config.subjectCode;
  document["room_code"] = config.roomCode;
  document["token"] = config.token;
  document["attendance_mode"] = config.attendanceMode;
  document["continuous_checking"] = config.continuousChecking;
  if (config.issuedAt != 0) document["issued_at"] = config.issuedAt;
  document["expires_at"] = config.expiresAt;
  String json;
  serializeJson(document, json);
  return json;
}

StatusCode AppConfig::validate(const SessionConfiguration& config) {
  if (config.command != "START_SESSION" || config.sessionId.isEmpty() || config.scheduleId == 0 ||
      config.subjectCode.isEmpty() || config.roomCode.isEmpty() || config.token.isEmpty() ||
      config.expiresAt < Constants::MIN_VALID_EPOCH ||
      (config.attendanceMode != "ble" && config.attendanceMode != "face" &&
       config.attendanceMode != "ble_and_face")) {
    return StatusCode::INVALID_PAYLOAD;
  }
  return StatusCode::OK;
}

const char* AppConfig::statusText(const StatusCode status) {
  switch (status) {
    case StatusCode::OK: return "OK";
    case StatusCode::INVALID_PAYLOAD: return "INVALID_PAYLOAD";
    case StatusCode::SESSION_EXPIRED: return "SESSION_EXPIRED";
    case StatusCode::DEVICE_BUSY: return "DEVICE_BUSY";
    case StatusCode::ALREADY_ACTIVE: return "ALREADY_ACTIVE";
    case StatusCode::AUTHENTICATED: return "AUTHENTICATED";
    case StatusCode::NOT_AUTHENTICATED: return "NOT_AUTHENTICATED";
    case StatusCode::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
    case StatusCode::ROOM_MISMATCH: return "ROOM_MISMATCH";
    case StatusCode::STORAGE_ERROR: return "STORAGE_ERROR";
    case StatusCode::ADVERTISEMENT_START_FAILED: return "ADVERTISEMENT_START_FAILED";
    case StatusCode::BLE_INITIALIZATION_FAILURE: return "BLE_INITIALIZATION_FAILURE";
    case StatusCode::NO_ACTIVE_SESSION: return "NO_ACTIVE_SESSION";
    case StatusCode::CLOCK_NOT_SET: return "CLOCK_NOT_SET";
    default: return "UNKNOWN";
  }
}
