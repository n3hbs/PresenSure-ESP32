# PresenSure ESP32 Attendance Beacon

ESP32 DevKit V1 firmware for the PresenSure BLE attendance flow. Laravel owns the
official attendance session, React Native transfers it, and the ESP32 broadcasts
only a temporary attendance proof.

## Device data

Permanent NVS data:

- `device_id`
- `device_name`
- `room_id`
- `device_secret`
- `provisioned`

`device_secret` is used internally for signature verification and is never exposed
over GATT, advertised, or printed. The initial room assignment is configured with
`DEFAULT_ROOM_ID` and `DEFAULT_ROOM_NAME` in `include/Constants.h`.

Temporary session data stays in RAM and is cleared on stop, expiry, or reboot:

- `session_id`
- `session_code`
- `token`
- `attendance_mode`
- `continuous_checking`
- `issued_at`
- `expires_at`
- `signature`
- active state

## GATT contract

All UUIDs are in `include/Constants.h`. Session and control writes require an
encrypted BLE connection.

| Characteristic | Access | Value |
|---|---|---|
| Device Information | Read | Public device JSON |
| Session Command | Encrypted write with response | `START_SESSION` JSON |
| Session Status | Read, notify | Command result JSON |
| Control | Encrypted write | `STOP_SESSION`, `GET_STATUS`, or `CLEAR_SESSION` JSON |

Device Information returns:

```json
{
  "device_id": "PS-1234ABCD",
  "device_name": "PresenSure-Room-301",
  "room_id": 12,
  "provisioned": true,
  "device_status": "ready"
}
```

Session Command accepts the required Laravel session proof plus optional settings:

```json
{
  "command": "START_SESSION",
  "session_id": "ATT-20260721-0001",
  "session_code": "A71F32",
  "token": "X8A3K92M",
  "attendance_mode": "ble_and_face",
  "continuous_checking": true,
  "issued_at": 1784592600,
  "expires_at": 1784596200,
  "signature": "64-lowercase-hex-HMAC-SHA256"
}
```

Minimum required fields are `command`, `session_id`, `token`, `expires_at`, and
`signature`. The signed UTF-8 value is:

```text
command|session_id|session_code|token|attendance_mode|continuous_as_0_or_1|issued_at|expires_at
```

The signature is lowercase HMAC-SHA256 hex using the device's raw 32-byte secret.

Stop a session with:

```json
{"command":"STOP_SESSION","session_id":"ATT-20260721-0001"}
```

## Student advertisement

The firmware sends a 23-byte manufacturer payload instead of JSON:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | Manufacturer ID |
| 2 | 1 | Protocol version |
| 3 | 1 | Attendance flags |
| 4 | 1 | Advertisement version |
| 5 | 4 | Session short ID |
| 9 | 4 | Time window (`epoch / 30`) |
| 13 | 6 | Rotating verification token |
| 19 | 4 | Device short ID |

Flags use bit 0 for BLE, bit 1 for face verification, and bit 2 for continuous
checking. The rotating proof is HMAC-SHA256 over `token|time_window`, truncated to
six bytes. Student, instructor, course, room-name, Laravel-token, and device-secret
data are never advertised.

## Build

```powershell
pio run
pio run --target upload
pio device monitor
```
