# Wi-Fi reconnection test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T5, T8 / connection recovery |
| Method | One device-originated disconnect and cleanup observation |
| Acceptance check | Detect the disconnect and regain an IP address without restarting the ESP32. |
| Outcome and scope | Recovery observed in about 4.83 seconds; router outage and endurance remain open. |

Baseline: a6509fe with a temporary device-disconnect test task.

## Procedure
After obtaining an IP address, the test task waited 15 seconds,
requested one ESP32 Wi-Fi disconnect, and deleted itself.
The shared router was not changed.

## Observed results
- Initial IP acquisition: 5414 ms.
- Test triggered: 20414 ms.
- Disconnection event: 20424 ms.
- Reconnection attempt: 21424 ms.
- IP reacquired: 25254 ms.
- Recovery took approximately 4.83 seconds from disconnection.

## Assessment
Automatic reconnection passed for a device-initiated disconnect.

## Limitations
- Router outage and prolonged network unavailability were not tested.
- Supplied excerpt does not establish sensing continuity during
  the disconnected interval.
- Internet service delivery was not tested.

## Cleanup

Temporary test code was removed and firmware uploaded again.

After cleanup, the supplied log covered approximately 172 seconds
after IP acquisition. No forced-disconnect message, Wi-Fi
disconnection, or restart appeared. Sensor readings continued every
five seconds, with no repeated WET notification event.
