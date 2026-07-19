#include "SecurityManager.h"

#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

Sha256Digest SecurityManager::sha256(const String& message) const {
  Sha256Digest digest{};
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  mbedtls_sha256_starts_ret(&context, 0);
  mbedtls_sha256_update_ret(&context, reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
  mbedtls_sha256_finish_ret(&context, digest.data());
  mbedtls_sha256_free(&context);
  return digest;
}

Sha256Digest SecurityManager::hmacSha256(const String& message, const String& secretHex) const {
  Sha256Digest digest{};
  std::array<uint8_t, Constants::DEVICE_SECRET_BYTES> secret{};
  if (!fromHex(secretHex, secret.data(), secret.size())) return digest;

  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info != nullptr) {
    mbedtls_md_hmac(info, secret.data(), secret.size(),
                    reinterpret_cast<const uint8_t*>(message.c_str()), message.length(), digest.data());
  }
  return digest;
}

bool SecurityManager::validateSessionSignature(const SessionConfiguration& config,
                                               const String& secretHex) const {
  const Sha256Digest expected = hmacSha256(AppConfig::canonicalPayload(config), secretHex);
  return constantTimeEquals(toHex(expected.data(), expected.size()), config.signature);
}

String SecurityManager::toHex(const uint8_t* bytes, const size_t length) {
  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  String result;
  result.reserve(length * 2);
  for (size_t index = 0; index < length; ++index) {
    result += HEX_DIGITS[bytes[index] >> 4U];
    result += HEX_DIGITS[bytes[index] & 0x0FU];
  }
  return result;
}

bool SecurityManager::fromHex(const String& hex, uint8_t* output, const size_t outputLength) {
  if (hex.length() != outputLength * 2) return false;
  auto nibble = [](const char value) -> int8_t {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (size_t index = 0; index < outputLength; ++index) {
    const int8_t high = nibble(hex[index * 2]);
    const int8_t low = nibble(hex[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

bool SecurityManager::constantTimeEquals(const String& left, const String& right) {
  if (left.length() != right.length()) return false;
  uint8_t difference = 0;
  for (size_t index = 0; index < left.length(); ++index) {
    difference |= static_cast<uint8_t>(left[index] ^ right[index]);
  }
  return difference == 0;
}
