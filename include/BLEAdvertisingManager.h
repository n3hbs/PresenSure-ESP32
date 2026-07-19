#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "SessionManager.h"
#include "StorageManager.h"
#include "TokenGenerator.h"

/** Builds and rotates privacy-preserving compact attendance advertisements. */
class BLEAdvertisingManager {
 public:
  BLEAdvertisingManager(SessionManager& sessions, const StorageManager& storage,
                        const TokenGenerator& tokens);

  /** Starts non-connectable attendance advertising for the active session. */
  StatusCode start();

  /** Refreshes the payload whenever the 30-second window changes. */
  StatusCode update();

  /** Stops attendance advertising. */
  void stop();

  /** Returns whether attendance advertising is enabled. */
  bool isAdvertising() const;

 private:
  bool applyPayload();
  std::array<uint8_t, 23> buildPayload(uint32_t timeWindow) const;
  static void appendUint32(std::array<uint8_t, 23>& payload, size_t offset, uint32_t value);

  SessionManager& sessions_;
  const StorageManager& storage_;
  const TokenGenerator& tokens_;
  uint32_t advertisedWindow_ = UINT32_MAX;
  bool advertising_ = false;
};

