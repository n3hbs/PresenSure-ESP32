#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "AppConfig.h"

/** Owns all non-volatile device and session persistence. */
class StorageManager {
 public:
  /** Opens the PresenSure NVS namespace and provisions identity if needed. */
  bool begin();

  /** Closes the NVS namespace. */
  void end();

  /** Returns the public device identifier. */
  const String& deviceId() const;

  const String& deviceName() const;

  uint32_t roomId() const;

  bool provisioned() const;

  /** Returns the private device key for internal cryptographic use only. */
  const String& deviceSecret() const;

 private:
  bool provisionIdentity();
  static String randomHex(size_t byteCount);

  Preferences preferences_;
  String deviceId_;
  String deviceName_;
  String deviceSecret_;
  uint32_t roomId_ = 0;
  bool provisioned_ = false;
  bool initialized_ = false;
};
