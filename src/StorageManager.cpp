#include "StorageManager.h"

#include <ArduinoJson.h>
#include <esp_system.h>

#include "Constants.h"
#include "Log.h"

namespace {
constexpr char DEVICE_ID_KEY[] = "device_id";
constexpr char DEVICE_SECRET_KEY[] = "device_key";
constexpr char SESSION_KEY[] = "session";
}

bool StorageManager::begin() {
  initialized_ = preferences_.begin(Constants::NVS_NAMESPACE, false);
  if (!initialized_) {
    Log::error("Unable to open NVS namespace");
    return false;
  }
  return provisionIdentity();
}

void StorageManager::end() {
  if (initialized_) {
    preferences_.end();
    initialized_ = false;
  }
}

const String& StorageManager::deviceId() const { return deviceId_; }

const String& StorageManager::deviceSecret() const { return deviceSecret_; }

bool StorageManager::saveSession(const SessionConfiguration& config) {
  if (!initialized_) return false;
  const String json = AppConfig::toJson(config);
  return preferences_.putString(SESSION_KEY, json) == json.length();
}

bool StorageManager::loadSession(SessionConfiguration& config) {
  if (!initialized_ || !preferences_.isKey(SESSION_KEY)) return false;
  const String json = preferences_.getString(SESSION_KEY, "");
  return !json.isEmpty() && AppConfig::fromJson(json, config) == StatusCode::OK;
}

bool StorageManager::clearSession() {
  if (!initialized_) return false;
  return !preferences_.isKey(SESSION_KEY) || preferences_.remove(SESSION_KEY);
}

bool StorageManager::provisionIdentity() {
  deviceId_ = preferences_.getString(DEVICE_ID_KEY, "");
  deviceSecret_ = preferences_.getString(DEVICE_SECRET_KEY, "");
  if (!deviceId_.isEmpty() && deviceSecret_.length() == Constants::DEVICE_SECRET_BYTES * 2) {
    return true;
  }

  const uint64_t chipId = ESP.getEfuseMac();
  char identifier[24];
  snprintf(identifier, sizeof(identifier), "PS-%08lX", static_cast<unsigned long>(chipId & 0xFFFFFFFFULL));
  deviceId_ = identifier;
  deviceSecret_ = randomHex(Constants::DEVICE_SECRET_BYTES);

  const bool idSaved = preferences_.putString(DEVICE_ID_KEY, deviceId_) == deviceId_.length();
  const bool secretSaved = preferences_.putString(DEVICE_SECRET_KEY, deviceSecret_) == deviceSecret_.length();
  if (!idSaved || !secretSaved) {
    Log::error("Device identity provisioning failed");
    return false;
  }
  Log::info("Provisioned device %s", deviceId_.c_str());
  return true;
}

String StorageManager::randomHex(const size_t byteCount) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  String result;
  result.reserve(byteCount * 2);
  for (size_t index = 0; index < byteCount; ++index) {
    const uint8_t value = static_cast<uint8_t>(esp_random());
    result += HEX_DIGITS[value >> 4U];
    result += HEX_DIGITS[value & 0x0FU];
  }
  return result;
}
