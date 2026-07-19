#include "AppConfig.h"

#include <ArduinoJson.h>

#include "Constants.h"

StatusCode AppConfig::fromJson(const String& json, SessionConfiguration& output) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, json);
  if (error) {
    return StatusCode::INVALID_CONFIGURATION;
  }

  constexpr const char* required[] = {"session_id",       "attendance_type", "start_time",
                                      "end_time",         "continuous",      "rotating_secret",
                                      "signature",        "advertisement_interval_ms"};
  for (const char* key : required) {
    if (document[key].isNull()) {
      return StatusCode::MISSING_PARAMETERS;
    }
  }

  output.sessionId = document["session_id"].as<String>();
  output.attendanceType = static_cast<AttendanceType>(document["attendance_type"].as<uint8_t>());
  output.startTime = document["start_time"].as<uint32_t>();
  output.endTime = document["end_time"].as<uint32_t>();
  output.continuousChecking = document["continuous"].as<bool>();
  output.rotatingSecret = document["rotating_secret"].as<String>();
  output.signature = document["signature"].as<String>();
  output.signature.toLowerCase();
  output.advertisementIntervalMs = document["advertisement_interval_ms"].as<uint16_t>();
  return validate(output);
}

String AppConfig::toJson(const SessionConfiguration& config) {
  JsonDocument document;
  document["session_id"] = config.sessionId;
  document["attendance_type"] = static_cast<uint8_t>(config.attendanceType);
  document["start_time"] = config.startTime;
  document["end_time"] = config.endTime;
  document["continuous"] = config.continuousChecking;
  document["rotating_secret"] = config.rotatingSecret;
  document["signature"] = config.signature;
  document["advertisement_interval_ms"] = config.advertisementIntervalMs;
  String json;
  serializeJson(document, json);
  return json;
}

StatusCode AppConfig::validate(const SessionConfiguration& config) {
  const uint8_t type = static_cast<uint8_t>(config.attendanceType);
  if (config.sessionId.isEmpty() || config.rotatingSecret.isEmpty() || config.signature.isEmpty()) {
    return StatusCode::MISSING_PARAMETERS;
  }
  if (type < static_cast<uint8_t>(AttendanceType::BLE) ||
      type > static_cast<uint8_t>(AttendanceType::BOTH) || config.startTime < Constants::MIN_VALID_EPOCH ||
      config.endTime <= config.startTime ||
      config.advertisementIntervalMs < Constants::MIN_ADVERTISEMENT_INTERVAL_MS ||
      config.advertisementIntervalMs > Constants::MAX_ADVERTISEMENT_INTERVAL_MS ||
      config.signature.length() != Constants::SIGNATURE_HEX_LENGTH) {
    return StatusCode::INVALID_CONFIGURATION;
  }
  return StatusCode::OK;
}

String AppConfig::canonicalPayload(const SessionConfiguration& config) {
  String payload;
  payload.reserve(config.sessionId.length() + config.rotatingSecret.length() + 80);
  payload += config.sessionId;
  payload += '|';
  payload += String(static_cast<uint8_t>(config.attendanceType));
  payload += '|';
  payload += String(config.startTime);
  payload += '|';
  payload += String(config.endTime);
  payload += '|';
  payload += config.continuousChecking ? '1' : '0';
  payload += '|';
  payload += config.rotatingSecret;
  payload += '|';
  payload += String(config.advertisementIntervalMs);
  return payload;
}

const char* AppConfig::statusText(const StatusCode status) {
  switch (status) {
    case StatusCode::OK: return "OK";
    case StatusCode::INVALID_CONFIGURATION: return "INVALID_CONFIGURATION";
    case StatusCode::EXPIRED_SESSION: return "EXPIRED_SESSION";
    case StatusCode::INVALID_SIGNATURE: return "INVALID_SIGNATURE";
    case StatusCode::MISSING_PARAMETERS: return "MISSING_PARAMETERS";
    case StatusCode::STORAGE_FAILURE: return "STORAGE_FAILURE";
    case StatusCode::BLE_INITIALIZATION_FAILURE: return "BLE_INITIALIZATION_FAILURE";
    case StatusCode::NO_ACTIVE_SESSION: return "NO_ACTIVE_SESSION";
    case StatusCode::CLOCK_NOT_SET: return "CLOCK_NOT_SET";
    default: return "UNKNOWN";
  }
}
