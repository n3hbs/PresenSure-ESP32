#include <Arduino.h>

#include "AppConfig.h"
#include "BLEAdvertisingManager.h"
#include "BLEConfigurationService.h"
#include "Log.h"
#include "SecurityManager.h"
#include "SessionManager.h"
#include "StorageManager.h"
#include "TokenGenerator.h"

namespace {
StorageManager storage;
SecurityManager security;
TokenGenerator tokenGenerator(security);
SessionManager sessionManager(storage, security);
BLEConfigurationService configurationService(sessionManager, storage);
BLEAdvertisingManager advertisingManager(sessionManager, storage, tokenGenerator);

bool initialized = false;
uint32_t lastMaintenanceMs = 0;
constexpr uint32_t MAINTENANCE_INTERVAL_MS = 250;

void enterConfigurationMode() {
  advertisingManager.stop();
  if (!configurationService.startConfigurationAdvertising()) {
    Log::error("Could not start configuration advertising");
  }
}

void enterAttendanceMode() {
  configurationService.stopAdvertising();
  configurationService.disconnectAll();
  const StatusCode result = advertisingManager.start();
  if (result != StatusCode::OK) {
    Log::error("Could not start attendance advertising: %s", AppConfig::statusText(result));
    enterConfigurationMode();
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Log::info("PresenSure attendance beacon booting");

  if (!storage.begin()) {
    Log::error("Fatal storage initialization failure");
    return;
  }
  Log::info("Device ID: %s", storage.deviceId().c_str());

  const StatusCode bleResult = configurationService.begin();
  if (bleResult != StatusCode::OK) {
    Log::error("Fatal BLE initialization failure");
    return;
  }

  const StatusCode restoreResult = sessionManager.restore();
  if (restoreResult == StatusCode::OK && sessionManager.isActive()) {
    Log::info("Restored active session");
    enterAttendanceMode();
  } else {
    if (restoreResult != StatusCode::NO_ACTIVE_SESSION) {
      Log::warning("Session not resumed: %s", AppConfig::statusText(restoreResult));
    }
    enterConfigurationMode();
  }
  initialized = true;
}

void loop() {
  if (!initialized) {
    delay(1000);
    return;
  }

  configurationService.update();
  if (sessionManager.isActive() && !advertisingManager.isAdvertising()) {
    enterAttendanceMode();
  } else if (!sessionManager.isActive() && advertisingManager.isAdvertising()) {
    enterConfigurationMode();
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastMaintenanceMs >= MAINTENANCE_INTERVAL_MS) {
    lastMaintenanceMs = nowMs;
    if (sessionManager.isActive()) {
      const StatusCode sessionResult = sessionManager.update();
      if (sessionResult == StatusCode::EXPIRED_SESSION) {
        Log::info("Session expired");
        configurationService.publishStatus(sessionResult, false);
        enterConfigurationMode();
      } else if (sessionResult == StatusCode::OK) {
        const StatusCode advertisingResult = advertisingManager.update();
        if (advertisingResult == StatusCode::BLE_INITIALIZATION_FAILURE) {
          Log::error("Unable to rotate BLE advertisement");
        }
      }
    }
  }
  delay(10);
}
