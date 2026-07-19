#pragma once

#include <Arduino.h>

#include <array>

#include "AppConfig.h"
#include "Constants.h"

using Sha256Digest = std::array<uint8_t, 32>;

/** Provides hashing, HMAC signing, and constant-time signature validation. */
class SecurityManager {
 public:
  /** Computes SHA-256 over an Arduino String. */
  Sha256Digest sha256(const String& message) const;

  /** Computes HMAC-SHA256 with a hex-encoded 256-bit device secret. */
  Sha256Digest hmacSha256(const String& message, const String& secretHex) const;

  /** Validates a session's canonical payload signature in constant time. */
  bool validateSessionSignature(const SessionConfiguration& config, const String& secretHex) const;

  /** Converts a byte range into lowercase hexadecimal. */
  static String toHex(const uint8_t* bytes, size_t length);

 private:
  static bool fromHex(const String& hex, uint8_t* output, size_t outputLength);
  static bool constantTimeEquals(const String& left, const String& right);
};

