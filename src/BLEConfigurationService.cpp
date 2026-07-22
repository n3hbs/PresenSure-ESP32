#include "BLEConfigurationService.h"

#include <ArduinoJson.h>

#include "Constants.h"
#include "Log.h"

class BLEConfigurationService::AuthenticationCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit AuthenticationCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
    owner_.queueAuthenticationCommand(String(characteristic->getValue()), connection.getConnHandle());
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::SessionCommandCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit SessionCommandCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connection) override {
    owner_.queueSessionCommand(String(characteristic->getValue()), connection.getConnHandle());
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::StatusCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit StatusCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& connection, uint16_t subValue) override {
    if ((subValue & 0x01U) == 0U) return;
    if (owner_.authenticatedConnectionHandle_ == connection.getConnHandle() &&
        !owner_.sessions_.isActive()) {
      owner_.publishStatus(StatusCode::AUTHENTICATED);
    } else {
      owner_.publishStatus(owner_.sessions_.isActive() ? StatusCode::OK
                                                       : StatusCode::NO_ACTIVE_SESSION);
    }
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::ServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { Log::info("Instructor connected"); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& connection, int reason) override {
    Log::info("Instructor disconnected (reason %d)", reason);
    owner_.handleDisconnect(connection.getConnHandle());
    if (!owner_.sessions_.isActive()) owner_.startConfigurationAdvertising();
  }

 private:
  BLEConfigurationService& owner_;
};

BLEConfigurationService::BLEConfigurationService(SessionManager& sessions,
                                                 const StorageManager& storage)
    : sessions_(sessions), storage_(storage) {}

StatusCode BLEConfigurationService::begin() {
  if (!NimBLEDevice::init(storage_.deviceName().c_str())) {
    return StatusCode::BLE_INITIALIZATION_FAILURE;
  }

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) return StatusCode::BLE_INITIALIZATION_FAILURE;
  serverCallbacks_ = new ServerCallbacks(*this);
  server_->setCallbacks(serverCallbacks_, false);

  NimBLEService* service = server_->createService(Constants::SERVICE_UUID);
  if (service == nullptr) return StatusCode::BLE_INITIALIZATION_FAILURE;

  NimBLECharacteristic* authentication = service->createCharacteristic(
      Constants::AUTH_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE, 256);
  NimBLECharacteristic* sessionCommand = service->createCharacteristic(
      Constants::SESSION_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE, 512);
  statusCharacteristic_ = service->createCharacteristic(
      Constants::STATUS_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 256);

  if (authentication == nullptr || sessionCommand == nullptr || statusCharacteristic_ == nullptr) {
    return StatusCode::BLE_INITIALIZATION_FAILURE;
  }

  authenticationCallbacks_ = new AuthenticationCallbacks(*this);
  sessionCommandCallbacks_ = new SessionCommandCallbacks(*this);
  statusCallbacks_ = new StatusCallbacks(*this);
  authentication->setCallbacks(authenticationCallbacks_);
  sessionCommand->setCallbacks(sessionCommandCallbacks_);
  statusCharacteristic_->setCallbacks(statusCallbacks_);

  publishStatus(StatusCode::NO_ACTIVE_SESSION, false);
  return server_->start() ? StatusCode::OK : StatusCode::BLE_INITIALIZATION_FAILURE;
}

void BLEConfigurationService::update() {
  if (authenticationCommandPending_) {
    authenticationCommandPending_ = false;
    if (authenticatedConnectionHandle_ == pendingAuthenticationHandle_) {
      authenticatedConnectionHandle_ = UINT16_MAX;
      if (!sessions_.isActive()) state_ = BeaconState::Configuration;
    }
    JsonDocument document;
    StatusCode result = StatusCode::AUTHENTICATION_FAILED;
    if (!deserializeJson(document, pendingAuthenticationCommand_)) {
      const String command = document["command"] | "";
      const String secret = document["secret"] | "";
      if (command == "AUTHENTICATE" && secret == Constants::DEVELOPMENT_SECRET) {
        authenticatedConnectionHandle_ = pendingAuthenticationHandle_;
        if (!sessions_.isActive()) state_ = BeaconState::Authenticated;
        result = StatusCode::AUTHENTICATED;
      }
    }
    pendingAuthenticationCommand_.clear();
    publishStatus(result);
    Log::info("Authentication result: %s", AppConfig::statusText(result));
  }

  if (sessionCommandPending_) {
    sessionCommandPending_ = false;
    StatusCode result = StatusCode::NOT_AUTHENTICATED;
    if (pendingSessionHandle_ == authenticatedConnectionHandle_) {
      JsonDocument document;
      if (!deserializeJson(document, pendingSessionCommand_)) {
        const String command = document["command"] | "";
        if (command == "STOP_SESSION") {
          const String sessionId = document["session_id"] | "";
          result = sessions_.hasSession() && sessionId == sessions_.current().sessionId
                       ? sessions_.stop()
                       : StatusCode::NO_ACTIVE_SESSION;
          if (result == StatusCode::OK) state_ = BeaconState::Configuration;
        } else {
          const String claimedDeviceId = document["device_id"] | "";
          const String receivedSessionId = document["session_id"] | "";
          const String receivedSubjectCode = document["subject_code"] | "";
          const String receivedRoomCode = document["room_code"] | "";
          const String receivedToken = document["token"] | "";
          Log::info("START_SESSION received:");
          Log::info("  command=%s", command.c_str());
          Log::info("  secret=%s", document["secret"].isNull() ? "not supplied" : "[REDACTED]");
          Log::info("  device_id=%s (actual=%s)",
                    claimedDeviceId.isEmpty() ? "not supplied" : claimedDeviceId.c_str(),
                    storage_.deviceId().c_str());
          Log::info("  session_id=%s", receivedSessionId.c_str());
          Log::info("  schedule_id=%lu",
                    static_cast<unsigned long>(document["schedule_id"] | 0U));
          Log::info("  subject_code=%s", receivedSubjectCode.c_str());
          Log::info("  room_code=%s", receivedRoomCode.c_str());
          Log::info("  token=[REDACTED] (%u bytes)",
                    static_cast<unsigned>(receivedToken.length()));
          Log::info("  issued_at=%lu",
                    static_cast<unsigned long>(document["issued_at"] | 0U));
          Log::info("  expires_at=%lu",
                    static_cast<unsigned long>(document["expires_at"] | 0U));
          SessionConfiguration config;
          result = AppConfig::fromJson(pendingSessionCommand_, config);
          if (result == StatusCode::OK) result = sessions_.configure(config);
          if (result == StatusCode::OK) result = sessions_.start();
          if (result == StatusCode::OK) state_ = BeaconState::SessionActive;
        }
      } else {
        result = StatusCode::INVALID_PAYLOAD;
      }
    }
    pendingSessionCommand_.clear();
    publishStatus(result);
    Log::info("Session command result: %s", AppConfig::statusText(result));
  }
}

bool BLEConfigurationService::startConfigurationAdvertising() {
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising == nullptr) return false;
  advertising->stop();
  advertising->reset();
  advertising->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  advertising->enableScanResponse(true);
  advertising->setMinInterval(160);
  advertising->setMaxInterval(240);

  NimBLEAdvertisementData advertisement;
  advertisement.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advertisement.setCompleteServices(NimBLEUUID(Constants::SERVICE_UUID));
  advertisement.setManufacturerData(storage_.roomCode().c_str());
  NimBLEAdvertisementData scanResponse;
  scanResponse.setName(storage_.deviceName().c_str());
  if (!advertising->setAdvertisementData(advertisement) ||
      !advertising->setScanResponseData(scanResponse)) return false;
  Log::info("Configuration advertising started for room %s", storage_.roomCode().c_str());
  return advertising->start();
}

void BLEConfigurationService::stopAdvertising() {
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising != nullptr) advertising->stop();
}

void BLEConfigurationService::disconnectAll() {
  if (server_ == nullptr) return;
  const uint8_t connectionCount = server_->getConnectedCount();
  for (uint8_t index = 0; index < connectionCount; ++index) {
    server_->disconnect(server_->getPeerInfo(index));
  }
}

bool BLEConfigurationService::hasConnectedClients() const {
  return server_ != nullptr && server_->getConnectedCount() > 0;
}

void BLEConfigurationService::publishStatus(const StatusCode status, const bool notify) {
  if (statusCharacteristic_ == nullptr) return;
  statusCharacteristic_->setValue(statusJson(status));
  if (notify && server_ != nullptr && server_->getConnectedCount() > 0) {
    statusCharacteristic_->notify();
  }
}

void BLEConfigurationService::queueAuthenticationCommand(const String& json,
                                                         const uint16_t connectionHandle) {
  pendingAuthenticationCommand_ = json;
  pendingAuthenticationHandle_ = connectionHandle;
  authenticationCommandPending_ = true;
}

void BLEConfigurationService::queueSessionCommand(const String& json,
                                                  const uint16_t connectionHandle) {
  pendingSessionCommand_ = json;
  pendingSessionHandle_ = connectionHandle;
  sessionCommandPending_ = true;
}

void BLEConfigurationService::handleDisconnect(const uint16_t connectionHandle) {
  if (authenticatedConnectionHandle_ == connectionHandle) {
    authenticatedConnectionHandle_ = UINT16_MAX;
  }
  state_ = sessions_.isActive() ? BeaconState::SessionActive : BeaconState::Configuration;
}

String BLEConfigurationService::statusJson(const StatusCode status) const {
  JsonDocument document;
  const bool success = status == StatusCode::OK || status == StatusCode::AUTHENTICATED;
  if (status == StatusCode::AUTHENTICATED) {
    document["status"] = "AUTHENTICATED";
  } else if (status == StatusCode::OK && sessions_.isActive()) {
    document["status"] = "SESSION_STARTED";
  } else if (status == StatusCode::OK) {
    document["status"] = "SESSION_STOPPED";
  } else if (status == StatusCode::NO_ACTIVE_SESSION) {
    document["status"] = "READY";
  } else {
    document["status"] = "ERROR";
  }
  document["success"] = success;
  document["device_id"] = storage_.deviceId();
  document["room_code"] = storage_.roomCode();
  if (sessions_.hasSession()) document["session_id"] = sessions_.current().sessionId;
  if (!success && status != StatusCode::NO_ACTIVE_SESSION) {
    document["code"] = AppConfig::statusText(status);
    document["message"] = String("BLE command failed: ") + AppConfig::statusText(status);
  }
  String json;
  serializeJson(document, json);
  return json;
}
