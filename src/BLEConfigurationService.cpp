#include "BLEConfigurationService.h"

#include <ArduinoJson.h>

#include "Constants.h"
#include "Log.h"

class BLEConfigurationService::SessionCommandCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit SessionCommandCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    owner_.queueSessionCommand(String(characteristic->getValue()));
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::ControlCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit ControlCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    owner_.queueControlCommand(String(characteristic->getValue()));
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::StatusCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit StatusCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
    // Android may enable notifications after START_SESSION was written. Send the
    // current state as soon as its CCCD subscription becomes active.
    if ((subValue & 0x01U) != 0U) {
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
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    Log::info("Instructor disconnected (reason %d)", reason);
    if (!owner_.sessions_.isActive()) owner_.startConfigurationAdvertising();
  }

 private:
  BLEConfigurationService& owner_;
};

BLEConfigurationService::BLEConfigurationService(SessionManager& sessions, const StorageManager& storage)
    : sessions_(sessions), storage_(storage) {}

StatusCode BLEConfigurationService::begin() {
  if (!NimBLEDevice::init(storage_.deviceName().c_str())) return StatusCode::BLE_INITIALIZATION_FAILURE;
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) return StatusCode::BLE_INITIALIZATION_FAILURE;
  serverCallbacks_ = new ServerCallbacks(*this);
  server_->setCallbacks(serverCallbacks_, false);

  NimBLEService* service = server_->createService(Constants::SERVICE_UUID);
  if (service == nullptr) return StatusCode::BLE_INITIALIZATION_FAILURE;

  NimBLECharacteristic* deviceInfo =
      service->createCharacteristic(Constants::DEVICE_INFO_UUID, NIMBLE_PROPERTY::READ, 256);
  NimBLECharacteristic* sessionCommand = service->createCharacteristic(
      Constants::SESSION_COMMAND_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 512);
  statusCharacteristic_ = service->createCharacteristic(
      Constants::STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 256);
  NimBLECharacteristic* control = service->createCharacteristic(
      Constants::CONTROL_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 256);

  if (deviceInfo == nullptr || sessionCommand == nullptr || statusCharacteristic_ == nullptr ||
      control == nullptr) return StatusCode::BLE_INITIALIZATION_FAILURE;

  sessionCommandCallbacks_ = new SessionCommandCallbacks(*this);
  controlCallbacks_ = new ControlCallbacks(*this);
  statusCallbacks_ = new StatusCallbacks(*this);
  sessionCommand->setCallbacks(sessionCommandCallbacks_);
  control->setCallbacks(controlCallbacks_);
  statusCharacteristic_->setCallbacks(statusCallbacks_);

  deviceInfo->setValue(deviceInfoJson());
  publishStatus(StatusCode::NO_ACTIVE_SESSION, false);
  return server_->start() ? StatusCode::OK : StatusCode::BLE_INITIALIZATION_FAILURE;
}

void BLEConfigurationService::update() {
  if (controlCommandPending_) {
    controlCommandPending_ = false;
    JsonDocument document;
    StatusCode result = StatusCode::INVALID_PAYLOAD;
    if (!deserializeJson(document, pendingControlCommand_)) {
      const String command = document["command"] | "";
      const String sessionId = document["session_id"] | "";
      if (command == "STOP_SESSION" && sessions_.hasSession() &&
          sessionId == sessions_.current().sessionId) {
        result = sessions_.stop();
      } else if (command == "CLEAR_SESSION") {
        result = sessions_.stop();
      } else if (command == "GET_STATUS") {
        result = sessions_.isActive() ? StatusCode::OK : StatusCode::NO_ACTIVE_SESSION;
      }
    }
    pendingControlCommand_.clear();
    publishStatus(result);
    Log::info("Control command: %s", AppConfig::statusText(result));
  }

  if (sessionCommandPending_) {
    sessionCommandPending_ = false;
    SessionConfiguration config;
    StatusCode result = AppConfig::fromJson(pendingSessionCommand_, config);
    pendingSessionCommand_.clear();
    if (result == StatusCode::OK) result = sessions_.configure(config);
    if (result == StatusCode::OK) result = sessions_.start();
    publishStatus(result);
    Log::info("START_SESSION result: %s", AppConfig::statusText(result));
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
  NimBLEAdvertisementData scanResponse;
  scanResponse.setName(storage_.deviceName().c_str());
  if (!advertising->setAdvertisementData(advertisement) ||
      !advertising->setScanResponseData(scanResponse)) return false;
  Log::info("Configuration advertising started");
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
  if (notify && server_ != nullptr && server_->getConnectedCount() > 0) statusCharacteristic_->notify();
}

void BLEConfigurationService::queueSessionCommand(const String& json) {
  pendingSessionCommand_ = json;
  sessionCommandPending_ = true;
}

void BLEConfigurationService::queueControlCommand(const String& json) {
  pendingControlCommand_ = json;
  controlCommandPending_ = true;
}

String BLEConfigurationService::deviceInfoJson() const {
  JsonDocument document;
  document["device_id"] = storage_.deviceId();
  document["device_name"] = storage_.deviceName();
  document["room_id"] = storage_.roomId();
  document["provisioned"] = storage_.provisioned();
  document["device_status"] = sessions_.isActive() ? "busy" : "ready";
  String json;
  serializeJson(document, json);
  return json;
}

String BLEConfigurationService::statusJson(const StatusCode status) const {
  JsonDocument document;
  document["success"] = status == StatusCode::OK;
  document["command"] = sessions_.hasSession() ? "START_SESSION" : "STOP_SESSION";
  if (sessions_.hasSession()) document["session_id"] = sessions_.current().sessionId;
  document["device_id"] = storage_.deviceId();
  document["advertising"] = sessions_.isActive();
  if (sessions_.isActive()) document["started_at"] = sessions_.currentEpoch();
  if (status == StatusCode::OK) {
    document["error_code"] = nullptr;
  } else {
    document["error_code"] = AppConfig::statusText(status);
    document["message"] = String("Session command failed: ") + AppConfig::statusText(status);
  }
  String json;
  serializeJson(document, json);
  return json;
}
