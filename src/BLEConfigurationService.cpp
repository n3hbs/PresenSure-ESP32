#include "BLEConfigurationService.h"

#include <ArduinoJson.h>

#include "Constants.h"
#include "Log.h"

class BLEConfigurationService::ConfigurationCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit ConfigurationCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    owner_.queueConfiguration(String(characteristic->getValue()));
  }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::StartCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit StartCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) override { owner_.queueStart(); }

 private:
  BLEConfigurationService& owner_;
};

class BLEConfigurationService::StopCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit StopCallbacks(BLEConfigurationService& owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) override { owner_.queueStop(); }

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
  String deviceName = Constants::DEVICE_NAME_PREFIX + storage_.deviceId();
  if (!NimBLEDevice::init(deviceName.c_str())) return StatusCode::BLE_INITIALIZATION_FAILURE;
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
  NimBLECharacteristic* configuration = service->createCharacteristic(
      Constants::CONFIGURATION_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 512);
  NimBLECharacteristic* start = service->createCharacteristic(
      Constants::START_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 16);
  NimBLECharacteristic* stop = service->createCharacteristic(
      Constants::STOP_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC, 16);
  statusCharacteristic_ = service->createCharacteristic(
      Constants::STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 256);
  NimBLECharacteristic* battery =
      service->createCharacteristic(Constants::BATTERY_UUID, NIMBLE_PROPERTY::READ, 1);
  NimBLECharacteristic* heartbeat =
      service->createCharacteristic(Constants::HEARTBEAT_UUID, NIMBLE_PROPERTY::READ, 16);

  if (deviceInfo == nullptr || configuration == nullptr || start == nullptr || stop == nullptr ||
      statusCharacteristic_ == nullptr || battery == nullptr || heartbeat == nullptr) {
    return StatusCode::BLE_INITIALIZATION_FAILURE;
  }

  configurationCallbacks_ = new ConfigurationCallbacks(*this);
  startCallbacks_ = new StartCallbacks(*this);
  stopCallbacks_ = new StopCallbacks(*this);
  configuration->setCallbacks(configurationCallbacks_);
  start->setCallbacks(startCallbacks_);
  stop->setCallbacks(stopCallbacks_);

  deviceInfo->setValue(deviceInfoJson());
  const uint8_t unknownBattery = 0xFF;
  battery->setValue(&unknownBattery, 1);
  heartbeat->setValue("reserved");
  publishStatus(StatusCode::NO_ACTIVE_SESSION, false);
  return server_->start() ? StatusCode::OK : StatusCode::BLE_INITIALIZATION_FAILURE;
}

void BLEConfigurationService::update() {
  if (stopPending_) {
    stopPending_ = false;
    const StatusCode result = sessions_.stop();
    publishStatus(result);
    Log::info("Stop command: %s", AppConfig::statusText(result));
  }

  if (configurationPending_) {
    configurationPending_ = false;
    const String json = pendingConfiguration_;
    pendingConfiguration_.clear();
    SessionConfiguration config;
    StatusCode result = AppConfig::fromJson(json, config);
    if (result == StatusCode::OK) result = sessions_.configure(config);
    if (result == StatusCode::OK) result = sessions_.start();
    publishStatus(result);
    Log::info("Configuration result: %s", AppConfig::statusText(result));
    if (result == StatusCode::OK) disconnectAll();
  }

  if (startPending_) {
    startPending_ = false;
    const StatusCode result = sessions_.start();
    publishStatus(result);
    Log::info("Start command: %s", AppConfig::statusText(result));
    if (result == StatusCode::OK) disconnectAll();
  }
}

bool BLEConfigurationService::startConfigurationAdvertising() {
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising == nullptr) return false;
  advertising->stop();
  advertising->reset();
  advertising->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  advertising->enableScanResponse(true);
  advertising->setMinInterval(160);  // 100 ms in 0.625 ms units.
  advertising->setMaxInterval(240);  // 150 ms in 0.625 ms units.

  NimBLEAdvertisementData advertisement;
  advertisement.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advertisement.setCompleteServices(NimBLEUUID(Constants::SERVICE_UUID));
  NimBLEAdvertisementData scanResponse;
  scanResponse.setName((Constants::DEVICE_NAME_PREFIX + storage_.deviceId()).c_str());
  if (!advertising->setAdvertisementData(advertisement) ||
      !advertising->setScanResponseData(scanResponse)) {
    return false;
  }
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

void BLEConfigurationService::publishStatus(const StatusCode status, const bool notify) {
  if (statusCharacteristic_ == nullptr) return;
  const String json = statusJson(status);
  statusCharacteristic_->setValue(json);
  if (notify && server_ != nullptr && server_->getConnectedCount() > 0) {
    statusCharacteristic_->notify();
  }
}

void BLEConfigurationService::queueConfiguration(const String& json) {
  pendingConfiguration_ = json;
  configurationPending_ = true;
}

void BLEConfigurationService::queueStart() { startPending_ = true; }

void BLEConfigurationService::queueStop() { stopPending_ = true; }

String BLEConfigurationService::deviceInfoJson() const {
  JsonDocument document;
  document["device_id"] = storage_.deviceId();
  document["protocol_version"] = Constants::PROTOCOL_VERSION;
  document["firmware_version"] = "1.0.0";
  document["platform"] = "ESP32";
  String json;
  serializeJson(document, json);
  return json;
}

String BLEConfigurationService::statusJson(const StatusCode status) const {
  JsonDocument document;
  document["code"] = static_cast<uint8_t>(status);
  document["status"] = AppConfig::statusText(status);
  document["active"] = sessions_.isActive();
  document["configured"] = sessions_.hasSession();
  document["epoch"] = sessions_.currentEpoch();
  if (sessions_.hasSession()) document["session_id"] = sessions_.current().sessionId;
  String json;
  serializeJson(document, json);
  return json;
}
