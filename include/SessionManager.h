#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "StorageManager.h"

/** Controls configuration acceptance, clock anchoring, activation, and expiry. */
class SessionManager {
 public:
  explicit SessionManager(StorageManager& storage);

  /** Validates the room and session payload, then stages it in RAM. */
  StatusCode configure(const SessionConfiguration& config);

  /** Activates the staged session when it is within its time range. */
  StatusCode start();

  /** Stops and removes the current session. */
  StatusCode stop();

  /** Expires an active session when its end time has passed. */
  StatusCode update();

  /** Returns whether attendance advertising should currently run. */
  bool isActive() const;

  /** Returns whether any validated session is staged. */
  bool hasSession() const;

  /** Returns the current session configuration. */
  const SessionConfiguration& current() const;

  /** Returns current Unix time, or zero if the clock is invalid. */
  uint32_t currentEpoch() const;

  /** Returns the current 30-second verification window number. */
  uint32_t currentTimeWindow() const;

 private:
  bool clockIsValid() const;
  void anchorClock(uint32_t issuedAt);

  StorageManager& storage_;
  SessionConfiguration current_;
  bool hasSession_ = false;
  bool active_ = false;
};
