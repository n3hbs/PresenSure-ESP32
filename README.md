# PresenSure ESP32 Attendance Beacon

ESP32 DevKit V1 firmware for the PresenSure BLE attendance flow. Laravel creates
the attendance session, React Native transfers it over BLE, and the ESP32
broadcasts a compact rotating attendance proof.

## BLE identity

- Device name: `PresenSure-ComLab-A`
- Permanent room code: `ComLab-A`
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`

Configuration advertisements include the room code as manufacturer data. The
status characteristic also returns `room_code`, so the app can validate the
permanent room assignment rather than trusting only the BLE name.

## Characteristics

| Characteristic | UUID | Access |
|---|---|---|
| Authentication | `beb5483e-36e1-4688-b7f5-ea07361b26a8` | Write with response |
| Session configuration | `beb5483e-36e1-4688-b7f5-ea07361b26a9` | Write with response |
| Device status | `beb5483e-36e1-4688-b7f5-ea07361b26aa` | Read, notify |

React Native must encode JSON as UTF-8 and then Base64 for
`react-native-ble-plx`. The ESP32 receives the original UTF-8 JSON bytes; it must
not Base64-decode them again.

Authenticate once per BLE connection:

```json
{"command":"AUTHENTICATE","secret":"presensure"}
```

The development secret is intentionally temporary. Replace it with a short-lived
Laravel configuration token before production use.

After receiving `AUTHENTICATED`, send:

```json
{
  "command": "START_SESSION",
  "session_id": "attendance-session-184",
  "schedule_id": 25,
  "subject_code": "IT101",
  "room_code": "ComLab-A",
  "token": "random-session-token",
  "issued_at": 1784696400,
  "expires_at": 1784700000
}
```

Required session fields are `session_id`, `schedule_id`, `subject_code`,
`room_code`, `token`, and `expires_at`. `issued_at` is also required on the first
session after boot when the ESP32 clock is not yet set. Use the current Unix time
in seconds.

To stop an active session, reconnect, authenticate, and write this to the session
characteristic:

```json
{"command":"STOP_SESSION","session_id":"attendance-session-184"}
```

Status values include `READY`, `AUTHENTICATED`, `SESSION_STARTED`,
`SESSION_STOPPED`, and `ERROR`. Error responses include `code` and `message`.
React Native should subscribe to status notifications before sending commands and
must wait for the matching success status before proceeding.

## Student advertisement

During an active session, the ESP32 remains connectable for authenticated stop
commands and broadcasts a 23-byte manufacturer payload:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | Manufacturer ID |
| 2 | 1 | Protocol version |
| 3 | 1 | Attendance flags |
| 4 | 1 | Advertisement version |
| 5 | 4 | Session ID hash |
| 9 | 4 | Time window (`epoch / 30`) |
| 13 | 6 | Rotating verification token |
| 19 | 4 | Device ID hash |

Room names, subject codes, session tokens, and device secrets are not exposed in
active attendance advertisements.

## Build

```powershell
pio run
pio run --target upload
pio device monitor
```
