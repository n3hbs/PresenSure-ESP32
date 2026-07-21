#include "AppConfig.h"

#include <ArduinoJson.h>

#include "Constants.h"

StatusCode AppConfig::fromJson(const String& json, SessionConfiguration& output) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, json);
  if (error) {
    return StatusCode::INVALID_PAYLOAD;
  }

  constexpr const char* required[] = {"command", "session_id", "token", "expires_at", "signature"};
  for (const char* key : required) {
    if (document[key].isNull()) {
      return StatusCode::INVALID_PAYLOAD;
    }
  }

  output.command = document["command"].as<String>();
  output.sessionId = document["session_id"].as<String>();
  output.sessionCode = document["session_code"] | "";
  output.token = document["token"].as<String>();
  output.attendanceMode = document["attendance_mode"] | "ble_and_face";
  output.continuousChecking = document["continuous_checking"] | false;
  output.issuedAt = document["issued_at"] | 0U;
  output.expiresAt = document["expires_at"].as<uint32_t>();
  output.signature = document["signature"].as<String>();
  output.signature.toLowerCase();
  return validate(output);
}

String AppConfig::toJson(const SessionConfiguration& config) {
  JsonDocument document;
  document["command"] = config.command;
  document["session_id"] = config.sessionId;
  if (!config.sessionCode.isEmpty()) document["session_code"] = config.sessionCode;
  document["token"] = config.token;
  document["attendance_mode"] = config.attendanceMode;
  document["continuous_checking"] = config.continuousChecking;
  if (config.issuedAt != 0) document["issued_at"] = config.issuedAt;
  document["expires_at"] = config.expiresAt;
  document["signature"] = config.signature;
  String json;
  serializeJson(document, json);
  return json;
}

StatusCode AppConfig::validate(const SessionConfiguration& config) {
  if (config.command != "START_SESSION" || config.sessionId.isEmpty() || config.token.isEmpty() ||
      config.expiresAt < Constants::MIN_VALID_EPOCH ||
      (config.attendanceMode != "ble" && config.attendanceMode != "face" &&
       config.attendanceMode != "ble_and_face") ||
      config.signature.length() != Constants::SIGNATURE_HEX_LENGTH) {
    return StatusCode::INVALID_PAYLOAD;
  }
  return StatusCode::OK;
}

String AppConfig::canonicalPayload(const SessionConfiguration& config) {
  String payload;
  payload.reserve(config.sessionId.length() + config.token.length() + 96);
  payload += config.command;
  payload += '|';
  payload += config.sessionId;
  payload += '|';
  payload += config.sessionCode;
  payload += '|';
  payload += config.token;
  payload += '|';
  payload += config.attendanceMode;
  payload += '|';
  payload += config.continuousChecking ? '1' : '0';
  payload += '|';
  payload += String(config.issuedAt);
  payload += '|';
  payload += String(config.expiresAt);
  return payload;
}

const char* AppConfig::statusText(const StatusCode status) {
  switch (status) {
    case StatusCode::OK: return "OK";
    case StatusCode::INVALID_PAYLOAD: return "INVALID_PAYLOAD";
    case StatusCode::INVALID_SIGNATURE: return "INVALID_SIGNATURE";
    case StatusCode::SESSION_EXPIRED: return "SESSION_EXPIRED";
    case StatusCode::DEVICE_BUSY: return "DEVICE_BUSY";
    case StatusCode::ALREADY_ACTIVE: return "ALREADY_ACTIVE";
    case StatusCode::STORAGE_ERROR: return "STORAGE_ERROR";
    case StatusCode::ADVERTISEMENT_START_FAILED: return "ADVERTISEMENT_START_FAILED";
    case StatusCode::BLE_INITIALIZATION_FAILURE: return "BLE_INITIALIZATION_FAILURE";
    case StatusCode::NO_ACTIVE_SESSION: return "NO_ACTIVE_SESSION";
    case StatusCode::CLOCK_NOT_SET: return "CLOCK_NOT_SET";
    default: return "UNKNOWN";
  }
}
