#pragma once

#include <Arduino.h>

namespace Constants {

constexpr char DEVICE_NAME_PREFIX[] = "PresenSure-";
constexpr char DEFAULT_ROOM_CODE[] = "Room 101";
constexpr uint32_t DEFAULT_ROOM_ID = 2;
constexpr char NVS_NAMESPACE[] = "presensure";
constexpr char DEVELOPMENT_SECRET[] = "presensure";

// PresenSure custom BLE service and characteristic UUIDs.
constexpr char SERVICE_UUID[] = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr char AUTH_CHARACTERISTIC_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
constexpr char SESSION_CHARACTERISTIC_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
constexpr char STATUS_CHARACTERISTIC_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26aa";

constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t ADVERTISEMENT_VERSION = 1;
constexpr uint16_t MANUFACTURER_ID = 0xFFFF;  // Replace after Bluetooth SIG assignment.
constexpr uint32_t TOKEN_WINDOW_SECONDS = 30;
constexpr uint16_t DEFAULT_ADVERTISEMENT_INTERVAL_MS = 500;
constexpr uint32_t MIN_VALID_EPOCH = 1609459200UL;  // 2021-01-01 UTC.
constexpr size_t SESSION_HASH_BYTES = 4;
constexpr size_t TOKEN_BYTES = 6;
constexpr size_t DEVICE_HASH_BYTES = 4;
constexpr size_t DEVICE_SECRET_BYTES = 32;

}  // namespace Constants
