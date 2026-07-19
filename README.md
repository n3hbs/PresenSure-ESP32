# PresenSure ESP32 Attendance Beacon

Production-oriented ESP32 DevKit V1 firmware for configuring an attendance session over BLE GATT and then broadcasting compact, rotating attendance advertisements. The beacon never contacts the Laravel API.

## Runtime flow

1. On first boot, the device creates a public device ID and a random 256-bit device secret in ESP32 NVS.
2. With no valid active session, it advertises a connectable `PresenSure-PS-...` configuration peripheral.
3. The instructor app pairs, reads Device Information, and writes a signed JSON configuration.
4. The ESP32 validates the fields and HMAC, persists the session, disconnects, and switches to non-connectable attendance advertising.
5. The advertised verification token changes every 30 seconds.
6. A stop command or session expiry clears the saved active session and returns the device to configuration mode.

## GATT contract

All UUIDs are centralized in `include/Constants.h`. Configuration, start, and stop writes require an encrypted BLE connection. Application-level HMAC validation is still mandatory.

| Characteristic | Access | Value |
|---|---|---|
| Device Information | Read | Public JSON device metadata |
| Configuration | Encrypted write | Signed configuration JSON |
| Start Advertising | Encrypted write | Any value triggers start |
| Stop Advertising | Encrypted write | Any value triggers stop |
| Device Status | Read, notify | JSON status code and state |
| Battery Level | Read | `0xFF` until implemented |
| Heartbeat | Read | Reserved for future use |

Configuration JSON:

```json
{
  "session_id": "attendance-session-uuid",
  "attendance_type": 1,
  "start_time": 1784422800,
  "end_time": 1784426400,
  "continuous": true,
  "rotating_secret": "per-session-random-value",
  "signature": "64-lowercase-hex-HMAC-SHA256",
  "advertisement_interval_ms": 500
}
```

Attendance types are `1 = BLE`, `2 = FACE`, and `3 = BOTH`. The advertisement interval must be from 100 to 5000 milliseconds.

The signature is lowercase hex HMAC-SHA256 using the device's raw 32-byte secret. The signed UTF-8 string has this exact order and separator:

```text
session_id|attendance_type|start_time|end_time|continuous_as_0_or_1|rotating_secret|advertisement_interval_ms
```

The device secret must be securely provisioned into the Laravel device registry during manufacturing/enrollment. It is intentionally never returned by GATT, included in advertisements, or printed to Serial.

## Attendance advertisement

The manufacturer-specific payload is 23 bytes, including the temporary development manufacturer ID. Multi-byte integers are little-endian.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | Manufacturer ID (`0xFFFF`, replace after assignment) |
| 2 | 1 | Protocol version |
| 3 | 1 | Attendance type |
| 4 | 1 | Advertisement version |
| 5 | 4 | First four bytes of SHA-256(session ID) |
| 9 | 4 | Unix time window (`epoch / 30`) |
| 13 | 6 | First six bytes of verification HMAC |
| 19 | 4 | First four bytes of SHA-256(device ID) |

The token input is the lowercase session-hash hex, attendance type, and decimal time window joined by `|`:

```text
hex_session_hash|attendance_type|time_window
```

The token is HMAC-SHA256 of that string using the raw device secret, truncated to six bytes.

## Clock limitation

This design has no Wi-Fi time synchronization or battery-backed RTC. When a new session arrives and the system clock is unset, firmware anchors Unix time to `start_time`, which assumes configuration occurs at session start. After total power loss, a saved session is not resumed unless the ESP32 clock remains valid. A future production revision should add a signed `current_time` field, NTP, or an external RTC.

## Build and upload

The environment is configured for a DOIT ESP32 DevKit V1 on `COM3`:

```powershell
pio run
pio run --target upload
pio device monitor
```

Serial logging runs at 115200 baud. Build output is written to `.pio/build/esp32dev/firmware.bin`.
