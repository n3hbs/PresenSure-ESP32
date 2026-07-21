#pragma once

#include <Arduino.h>

namespace Constants {

constexpr char DEVICE_NAME_PREFIX[] = "PresenSure-";
constexpr char DEFAULT_ROOM_NAME[] = "Room 201";
constexpr uint32_t DEFAULT_ROOM_ID = 2;
constexpr char NVS_NAMESPACE[] = "presensure";

// PresenSure custom BLE service and characteristic UUIDs.
constexpr char SERVICE_UUID[] = "76b50000-a1b2-c3d4-e5f6-1234567890ab";
constexpr char DEVICE_INFO_UUID[] = "76b50001-a1b2-c3d4-e5f6-1234567890ab";
constexpr char SESSION_COMMAND_UUID[] = "76b50002-a1b2-c3d4-e5f6-1234567890ab";
constexpr char CONTROL_UUID[] = "76b50003-a1b2-c3d4-e5f6-1234567890ab";
constexpr char STATUS_UUID[] = "76b50005-a1b2-c3d4-e5f6-1234567890ab";
constexpr char BATTERY_UUID[] = "76b50006-a1b2-c3d4-e5f6-1234567890ab";
constexpr char HEARTBEAT_UUID[] = "76b50007-a1b2-c3d4-e5f6-1234567890ab";

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
constexpr size_t SIGNATURE_HEX_LENGTH = 64;

}  // namespace Constants
