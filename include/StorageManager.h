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

  /** Returns the private device key for internal cryptographic use only. */
  const String& deviceSecret() const;

  /** Persists a validated active session. */
  bool saveSession(const SessionConfiguration& config);

  /** Loads the last saved active session. */
  bool loadSession(SessionConfiguration& config);

  /** Removes the active session record. */
  bool clearSession();

 private:
  bool provisionIdentity();
  static String randomHex(size_t byteCount);

  Preferences preferences_;
  String deviceId_;
  String deviceSecret_;
  bool initialized_ = false;
};
