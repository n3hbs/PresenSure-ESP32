#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "AppConfig.h"
#include "SessionManager.h"
#include "StorageManager.h"

/** Exposes the authenticated GATT configuration and control interface. */
class BLEConfigurationService {
 public:
  BLEConfigurationService(SessionManager& sessions, const StorageManager& storage);

  /** Creates the NimBLE server, service, and all required characteristics. */
  StatusCode begin();

  /** Processes deferred configuration/control writes from the BLE host task. */
  void update();

  /** Starts connectable advertising for instructor configuration. */
  bool startConfigurationAdvertising();

  /** Stops any currently active BLE advertising. */
  void stopAdvertising();

  /** Disconnects every connected instructor client. */
  void disconnectAll();

  /** Updates and optionally notifies the JSON device-status characteristic. */
  void publishStatus(StatusCode status, bool notify = true);

 private:
  class ConfigurationCallbacks;
  class StartCallbacks;
  class StopCallbacks;
  class ServerCallbacks;

  void queueConfiguration(const String& json);
  void queueStart();
  void queueStop();
  String deviceInfoJson() const;
  String statusJson(StatusCode status) const;

  SessionManager& sessions_;
  const StorageManager& storage_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* statusCharacteristic_ = nullptr;
  String pendingConfiguration_;
  volatile bool configurationPending_ = false;
  volatile bool startPending_ = false;
  volatile bool stopPending_ = false;
  ConfigurationCallbacks* configurationCallbacks_ = nullptr;
  StartCallbacks* startCallbacks_ = nullptr;
  StopCallbacks* stopCallbacks_ = nullptr;
  ServerCallbacks* serverCallbacks_ = nullptr;
};

