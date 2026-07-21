#include "StorageManager.h"

#include <ArduinoJson.h>
#include <esp_system.h>

#include "Constants.h"
#include "Log.h"

namespace
{
  constexpr char DEVICE_ID_KEY[] = "device_id";
  constexpr char DEVICE_SECRET_KEY[] = "device_key";
  constexpr char DEVICE_NAME_KEY[] = "device_name";
  constexpr char ROOM_ID_KEY[] = "room_id";
  constexpr char PROVISIONED_KEY[] = "provisioned";
}

bool StorageManager::begin()
{
  initialized_ = preferences_.begin(Constants::NVS_NAMESPACE, false);
  if (!initialized_)
  {
    Log::error("Unable to open NVS namespace");
    return false;
  }
  return provisionIdentity();
}

void StorageManager::end()
{
  if (initialized_)
  {
    preferences_.end();
    initialized_ = false;
  }
}

const String &StorageManager::deviceId() const { return deviceId_; }

const String &StorageManager::deviceName() const { return deviceName_; }

uint32_t StorageManager::roomId() const { return roomId_; }

bool StorageManager::provisioned() const { return provisioned_; }

const String &StorageManager::deviceSecret() const { return deviceSecret_; }

bool StorageManager::provisionIdentity()
{
  deviceId_ = preferences_.getString(DEVICE_ID_KEY, "");
  deviceSecret_ = preferences_.getString(DEVICE_SECRET_KEY, "");
  deviceName_ = preferences_.getString(DEVICE_NAME_KEY, "");
  roomId_ = preferences_.getUInt(ROOM_ID_KEY, 0);
  provisioned_ = preferences_.getBool(PROVISIONED_KEY, false);
  if (!deviceId_.isEmpty() && !deviceName_.isEmpty() && roomId_ != 0 &&
      deviceSecret_.length() == Constants::DEVICE_SECRET_BYTES * 2 && provisioned_)
  {

    const String configuredName =
        String(Constants::DEVICE_NAME_PREFIX) + Constants::DEFAULT_ROOM_NAME;

    if (roomId_ != Constants::DEFAULT_ROOM_ID)
    {
      roomId_ = Constants::DEFAULT_ROOM_ID;
      preferences_.putUInt(ROOM_ID_KEY, roomId_);
    }

    if (deviceName_ != configuredName)
    {
      deviceName_ = configuredName;
      preferences_.putString(DEVICE_NAME_KEY, deviceName_);
    }

    return true;
  }

  const uint64_t chipId = ESP.getEfuseMac();
  char identifier[24];
  snprintf(identifier, sizeof(identifier), "PS-%08lX", static_cast<unsigned long>(chipId & 0xFFFFFFFFULL));
  deviceId_ = identifier;
  deviceName_ = String(Constants::DEVICE_NAME_PREFIX) + Constants::DEFAULT_ROOM_NAME;
  roomId_ = Constants::DEFAULT_ROOM_ID;
  deviceSecret_ = randomHex(Constants::DEVICE_SECRET_BYTES);

  const bool idSaved = preferences_.putString(DEVICE_ID_KEY, deviceId_) == deviceId_.length();
  const bool secretSaved = preferences_.putString(DEVICE_SECRET_KEY, deviceSecret_) == deviceSecret_.length();
  const bool nameSaved = preferences_.putString(DEVICE_NAME_KEY, deviceName_) == deviceName_.length();
  const bool roomSaved = preferences_.putUInt(ROOM_ID_KEY, roomId_) > 0;
  const bool provisionedSaved = preferences_.putBool(PROVISIONED_KEY, true) > 0;
  if (!idSaved || !secretSaved || !nameSaved || !roomSaved || !provisionedSaved)
  {
    Log::error("Device identity provisioning failed");
    return false;
  }
  provisioned_ = true;
  Log::info("Provisioned device %s", deviceId_.c_str());
  return true;
}

String StorageManager::randomHex(const size_t byteCount)
{
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  String result;
  result.reserve(byteCount * 2);
  for (size_t index = 0; index < byteCount; ++index)
  {
    const uint8_t value = static_cast<uint8_t>(esp_random());
    result += HEX_DIGITS[value >> 4U];
    result += HEX_DIGITS[value & 0x0FU];
  }
  return result;
}
