#include "SessionManager.h"

#include <sys/time.h>
#include <time.h>

#include "Constants.h"
#include "Log.h"

SessionManager::SessionManager(StorageManager& storage, const SecurityManager& security)
    : storage_(storage), security_(security) {}

StatusCode SessionManager::restore() {
  SessionConfiguration restored;
  if (!storage_.loadSession(restored)) return StatusCode::NO_ACTIVE_SESSION;
  if (!security_.validateSessionSignature(restored, storage_.deviceSecret())) {
    storage_.clearSession();
    return StatusCode::INVALID_SIGNATURE;
  }
  current_ = restored;
  hasSession_ = true;
  if (!clockIsValid()) return StatusCode::CLOCK_NOT_SET;
  return start();
}

StatusCode SessionManager::configure(const SessionConfiguration& config) {
  const StatusCode validation = AppConfig::validate(config);
  if (validation != StatusCode::OK) return validation;
  if (!security_.validateSessionSignature(config, storage_.deviceSecret())) {
    return StatusCode::INVALID_SIGNATURE;
  }
  current_ = config;
  hasSession_ = true;
  active_ = false;
  if (!clockIsValid()) {
    anchorClockToSessionStart();
    Log::warning("Clock was unset; anchored to session start time");
  }
  if (!storage_.saveSession(current_)) {
    hasSession_ = false;
    return StatusCode::STORAGE_FAILURE;
  }
  return StatusCode::OK;
}

StatusCode SessionManager::start() {
  if (!hasSession_) return StatusCode::NO_ACTIVE_SESSION;
  const uint32_t now = currentEpoch();
  if (now == 0) return StatusCode::CLOCK_NOT_SET;
  if (now > current_.endTime) {
    stop();
    return StatusCode::EXPIRED_SESSION;
  }
  if (now < current_.startTime) return StatusCode::INVALID_CONFIGURATION;
  active_ = true;
  return StatusCode::OK;
}

StatusCode SessionManager::stop() {
  active_ = false;
  hasSession_ = false;
  current_ = SessionConfiguration{};
  return storage_.clearSession() ? StatusCode::OK : StatusCode::STORAGE_FAILURE;
}

StatusCode SessionManager::update() {
  if (!active_) return StatusCode::NO_ACTIVE_SESSION;
  const uint32_t now = currentEpoch();
  if (now == 0) {
    active_ = false;
    return StatusCode::CLOCK_NOT_SET;
  }
  if (now > current_.endTime) {
    stop();
    return StatusCode::EXPIRED_SESSION;
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

void SessionManager::anchorClockToSessionStart() {
  const timeval value{static_cast<time_t>(current_.startTime), 0};
  settimeofday(&value, nullptr);
}
