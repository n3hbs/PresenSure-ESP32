#pragma once

#include <Arduino.h>

#include <array>

#include "AppConfig.h"
#include "Constants.h"
#include "SecurityManager.h"

using SessionHash = std::array<uint8_t, Constants::SESSION_HASH_BYTES>;
using VerificationToken = std::array<uint8_t, Constants::TOKEN_BYTES>;
using DeviceHash = std::array<uint8_t, Constants::DEVICE_HASH_BYTES>;

/** Derives compact, non-reversible identifiers and rotating verification tokens. */
class TokenGenerator {
 public:
  explicit TokenGenerator(const SecurityManager& security);

  /** Generates the compact session hash sent in advertisements. */
  SessionHash sessionHash(const String& sessionId) const;

  /** Generates the compact public device identifier hash. */
  DeviceHash deviceHash(const String& deviceId) const;

  /** Generates a truncated HMAC token for the supplied 30-second time window. */
  VerificationToken token(const String& sessionToken, uint32_t timeWindow,
                          const String& deviceSecret) const;

 private:
  const SecurityManager& security_;
};
