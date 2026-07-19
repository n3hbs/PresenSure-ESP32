#include "TokenGenerator.h"

#include <cstring>

TokenGenerator::TokenGenerator(const SecurityManager& security) : security_(security) {}

SessionHash TokenGenerator::sessionHash(const String& sessionId) const {
  const Sha256Digest fullHash = security_.sha256(sessionId);
  SessionHash output{};
  std::copy_n(fullHash.begin(), output.size(), output.begin());
  return output;
}

DeviceHash TokenGenerator::deviceHash(const String& deviceId) const {
  const Sha256Digest fullHash = security_.sha256(deviceId);
  DeviceHash output{};
  std::copy_n(fullHash.begin(), output.size(), output.begin());
  return output;
}

VerificationToken TokenGenerator::token(const SessionHash& sessionHashValue, const AttendanceType type,
                                        const uint32_t timeWindow, const String& deviceSecret) const {
  String message = SecurityManager::toHex(sessionHashValue.data(), sessionHashValue.size());
  message += '|';
  message += String(static_cast<uint8_t>(type));
  message += '|';
  message += String(timeWindow);
  const Sha256Digest hmac = security_.hmacSha256(message, deviceSecret);
  VerificationToken output{};
  std::copy_n(hmac.begin(), output.size(), output.begin());
  return output;
}

