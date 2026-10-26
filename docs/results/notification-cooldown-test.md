# Notification cooldown test

## Procedure
Without restarting the ESP32, hold the potentiometer
at DRY, then NORMAL, then DRY until each state confirms.

## Observed results
- At 12.503 s, DRY confirmed and all four services accepted delivery.
- At 27.503 s, NORMAL confirmed and all four services accepted delivery.
- At 42.503 s, DRY confirmed again, but its notification was suppressed.

Serial evidence:
    I (42503) MONITOR: Confirmed state: DRY
    I (42503) NOTIFY: Cooldown suppressed repeated DRY alert

## Conclusion
Repeated DRY notification suppression passed within the
15-minute cooldown. NORMAL recovery notifications remained enabled.

## Limits
This hardware test did not wait 15 minutes.
Cooldown expiry was checked separately using unit tests.
Cooldown starts when an event enters the notification queue
and resets when the device restarts.