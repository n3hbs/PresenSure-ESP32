#include "SessionManager.h"

#include <sys/time.h>
#include <time.h>

#include "Constants.h"
#include "Log.h"

SessionManager::SessionManager(StorageManager& storage) : storage_(storage) {}

StatusCode SessionManager::configure(const SessionConfiguration& config) {
  if (active_) return current_.sessionId == config.sessionId ? StatusCode::ALREADY_ACTIVE : StatusCode::DEVICE_BUSY;
  const StatusCode validation = AppConfig::validate(config);
  if (validation != StatusCode::OK) return validation;
  if (config.roomCode != storage_.roomCode()) return StatusCode::ROOM_MISMATCH;
  if (!clockIsValid()) {
    if (config.issuedAt == 0) return StatusCode::CLOCK_NOT_SET;
    anchorClock(config.issuedAt);
    Log::warning("Clock was unset; anchored to phone-supplied issued_at");
  }
  if (currentEpoch() > config.expiresAt) return StatusCode::SESSION_EXPIRED;
  current_ = config;
  hasSession_ = true;
  active_ = false;
  return StatusCode::OK;
}

StatusCode SessionManager::start() {
  if (!hasSession_) return StatusCode::NO_ACTIVE_SESSION;
  const uint32_t now = currentEpoch();
  if (now == 0) return StatusCode::CLOCK_NOT_SET;
  if (now > current_.expiresAt) {
    stop();
    return StatusCode::SESSION_EXPIRED;
  }
  active_ = true;
  current_.active = true;
  return StatusCode::OK;
}

StatusCode SessionManager::stop() {
  active_ = false;
  hasSession_ = false;
  current_ = SessionConfiguration{};
  return StatusCode::OK;
}

StatusCode SessionManager::update() {
  if (!active_) return StatusCode::NO_ACTIVE_SESSION;
  const uint32_t now = currentEpoch();
  if (now == 0) {
    active_ = false;
    return StatusCode::CLOCK_NOT_SET;
  }
  if (now > current_.expiresAt) {
    stop();
    return StatusCode::SESSION_EXPIRED;
  }
  return StatusCode::OK;
}

bool SessionManager::isActive() const { return active_; }

bool SessionManager::hasSession() const { return hasSession_; }

const SessionConfiguration& SessionManager::current() const { return current_; }

uint32_t SessionManager::currentEpoch() const {
  const time_t now = time(nullptr);
  return now >= static_cast<time_t>(Constants::MIN_VALID_EPOCH) ? static_cast<uint32_t>(now) : 0;
}

uint32_t SessionManager::currentTimeWindow() const {
  return currentEpoch() / Constants::TOKEN_WINDOW_SECONDS;
}

bool SessionManager::clockIsValid() const { return currentEpoch() != 0; }

void SessionManager::anchorClock(const uint32_t issuedAt) {
  const timeval value{static_cast<time_t>(issuedAt), 0};
  settimeofday(&value, nullptr);
}
