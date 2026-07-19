#include "BLEAdvertisingManager.h"

#include <NimBLEDevice.h>

#include <algorithm>

#include "Constants.h"
#include "Log.h"

BLEAdvertisingManager::BLEAdvertisingManager(SessionManager& sessions, const StorageManager& storage,
                                             const TokenGenerator& tokens)
    : sessions_(sessions), storage_(storage), tokens_(tokens) {}

StatusCode BLEAdvertisingManager::start() {
  if (!sessions_.isActive()) return StatusCode::NO_ACTIVE_SESSION;
  advertisedWindow_ = UINT32_MAX;
  if (!applyPayload()) return StatusCode::BLE_INITIALIZATION_FAILURE;
  advertising_ = true;
  Log::info("Attendance advertising started");
  return StatusCode::OK;
}

StatusCode BLEAdvertisingManager::update() {
  if (!advertising_) return StatusCode::NO_ACTIVE_SESSION;
  if (!sessions_.isActive()) {
    stop();
    return StatusCode::EXPIRED_SESSION;
  }
  if (sessions_.currentTimeWindow() != advertisedWindow_ && !applyPayload()) {
    return StatusCode::BLE_INITIALIZATION_FAILURE;
  }
  return StatusCode::OK;
}

void BLEAdvertisingManager::stop() {
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising != nullptr) advertising->stop();
  advertising_ = false;
  advertisedWindow_ = UINT32_MAX;
}

bool BLEAdvertisingManager::isAdvertising() const { return advertising_; }

bool BLEAdvertisingManager::applyPayload() {
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  if (advertising == nullptr) return false;
  const uint32_t timeWindow = sessions_.currentTimeWindow();
  const auto payload = buildPayload(timeWindow);
  const uint16_t interval = static_cast<uint16_t>(
      (static_cast<uint32_t>(sessions_.current().advertisementIntervalMs) * 8U) / 5U);

  advertising->stop();
  advertising->reset();
  advertising->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  advertising->enableScanResponse(false);
  advertising->setMinInterval(interval);
  advertising->setMaxInterval(interval);
  NimBLEAdvertisementData data;
  data.setFlags(BLE_HS_ADV_F_BREDR_UNSUP);
  if (!data.setManufacturerData(payload.data(), payload.size()) ||
      !advertising->setAdvertisementData(data) || !advertising->start()) {
    return false;
  }
  advertisedWindow_ = timeWindow;
  Log::debug("Advertisement token rotated for window %lu", static_cast<unsigned long>(timeWindow));
  return true;
}

std::array<uint8_t, 23> BLEAdvertisingManager::buildPayload(const uint32_t timeWindow) const {
  std::array<uint8_t, 23> payload{};
  payload[0] = static_cast<uint8_t>(Constants::MANUFACTURER_ID & 0xFFU);
  payload[1] = static_cast<uint8_t>(Constants::MANUFACTURER_ID >> 8U);
  payload[2] = Constants::PROTOCOL_VERSION;
  payload[3] = static_cast<uint8_t>(sessions_.current().attendanceType);
  payload[4] = Constants::ADVERTISEMENT_VERSION;
  const SessionHash sessionHash = tokens_.sessionHash(sessions_.current().sessionId);
  std::copy(sessionHash.begin(), sessionHash.end(), payload.begin() + 5);
  appendUint32(payload, 9, timeWindow);
  const VerificationToken token = tokens_.token(sessionHash, sessions_.current().attendanceType, timeWindow,
                                                 storage_.deviceSecret());
  std::copy(token.begin(), token.end(), payload.begin() + 13);
  const DeviceHash deviceHash = tokens_.deviceHash(storage_.deviceId());
  std::copy(deviceHash.begin(), deviceHash.end(), payload.begin() + 19);
  return payload;
}

void BLEAdvertisingManager::appendUint32(std::array<uint8_t, 23>& payload, const size_t offset,
                                         const uint32_t value) {
  payload[offset] = static_cast<uint8_t>(value);
  payload[offset + 1] = static_cast<uint8_t>(value >> 8U);
  payload[offset + 2] = static_cast<uint8_t>(value >> 16U);
  payload[offset + 3] = static_cast<uint8_t>(value >> 24U);
}
