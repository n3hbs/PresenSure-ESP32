#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "AppConfig.h"
#include "SessionManager.h"
#include "StorageManager.h"

enum class BeaconState : uint8_t { Configuration, Authenticated, SessionActive };

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
  class AuthenticationCallbacks;
  class SessionCommandCallbacks;
  class StatusCallbacks;
  class ServerCallbacks;

  void queueAuthenticationCommand(const String& json, uint16_t connectionHandle);
  void queueSessionCommand(const String& json, uint16_t connectionHandle);
  void handleDisconnect(uint16_t connectionHandle);
  String statusJson(StatusCode status) const;

  SessionManager& sessions_;
  const StorageManager& storage_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* statusCharacteristic_ = nullptr;
  String pendingAuthenticationCommand_;
  String pendingSessionCommand_;
  uint16_t pendingAuthenticationHandle_ = UINT16_MAX;
  uint16_t pendingSessionHandle_ = UINT16_MAX;
  uint16_t authenticatedConnectionHandle_ = UINT16_MAX;
  volatile bool authenticationCommandPending_ = false;
  volatile bool sessionCommandPending_ = false;
  BeaconState state_ = BeaconState::Configuration;
  AuthenticationCallbacks* authenticationCallbacks_ = nullptr;
  SessionCommandCallbacks* sessionCommandCallbacks_ = nullptr;
  StatusCallbacks* statusCallbacks_ = nullptr;
  ServerCallbacks* serverCallbacks_ = nullptr;
};
