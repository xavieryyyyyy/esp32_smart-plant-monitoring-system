# Monitoring sample freshness test

## Policy
Each sample carries a monotonic acquisition-start timestamp.
Monitoring rejects samples older than 10 seconds or with future
timestamps. Rejection resets the confirmation streak.

## Controlled test
Temporarily subtracted 11 seconds from the third sample's timestamp.

Observed:
- 12.503 s: stale sample rejected; streak reset.
- 17.513 s: fresh reading counted 1/3.
- 22.513 s: fresh reading counted 2/3.
- 27.513 s: fresh reading counted 3/3; DRY confirmed.

## Cleanup verification
Removed the temporary timestamp modification and uploaded again.
DRY confirmed at 12.513 s without a stale warning.
All four notification services accepted the event.

## Limits
This injected an old timestamp; it did not create an actual queue backlog.
Future-timestamp rejection and the exact 10-second boundary were not tested.
This guard covers monitoring input, not notification delivery age.