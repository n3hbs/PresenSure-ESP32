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

  /** Returns true while an instructor app still has a GATT connection. */
  bool hasConnectedClients() const;

  /** Updates and optionally notifies the JSON device-status characteristic. */
  void publishStatus(StatusCode status, bool notify = true);

 private:
  class SessionCommandCallbacks;
  class ControlCallbacks;
  class StatusCallbacks;
  class ServerCallbacks;

  void queueSessionCommand(const String& json);
  void queueControlCommand(const String& json);
  String deviceInfoJson() const;
  String statusJson(StatusCode status) const;

  SessionManager& sessions_;
  const StorageManager& storage_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* statusCharacteristic_ = nullptr;
  String pendingSessionCommand_;
  String pendingControlCommand_;
  volatile bool sessionCommandPending_ = false;
  volatile bool controlCommandPending_ = false;
  SessionCommandCallbacks* sessionCommandCallbacks_ = nullptr;
  ControlCallbacks* controlCallbacks_ = nullptr;
  StatusCallbacks* statusCallbacks_ = nullptr;
  ServerCallbacks* serverCallbacks_ = nullptr;
};
