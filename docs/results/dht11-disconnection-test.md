## Captured results

### DHT11 signal disconnected

Four successive reads timed out. Potentiometer readings continued,
and DRY was confirmed after three valid ADC samples.

```text
I (2296) POT: Raw: 391 | Simulated moisture: 9% | State: DRY
I (2296) MONITOR: Candidate: DRY | Count: 1/3
E (2296) DHT11: Read failed: ESP_ERR_TIMEOUT
I (7296) POT: Raw: 382 | Simulated moisture: 9% | State: DRY
I (7296) MONITOR: Candidate: DRY | Count: 2/3
E (7296) DHT11: Read failed: ESP_ERR_TIMEOUT
I (12296) POT: Raw: 394 | Simulated moisture: 9% | State: DRY
I (12296) MONITOR: Candidate: DRY | Count: 3/3
I (12296) MONITOR: Confirmed state: DRY
E (12296) DHT11: Read failed: ESP_ERR_TIMEOUT
I (17296) POT: Raw: 393 | Simulated moisture: 9% | State: DRY
I (17296) MONITOR: Candidate: DRY | Count: 3/3
E (17296) DHT11: Read failed: ESP_ERR_TIMEOUT
```

### After reconnecting the signal

Two successful readings were captured five seconds apart.

```text
I (32296) POT: Raw: 392 | Simulated moisture: 9% | State: DRY
I (32296) MONITOR: Candidate: DRY | Count: 3/3
I (32296) DHT11: Temperature: 30.0 C | Humidity: 74.0 %
I (37296) POT: Raw: 391 | Simulated moisture: 9% | State: DRY
I (37296) MONITOR: Candidate: DRY | Count: 3/3
I (37296) DHT11: Temperature: 30.0 C | Humidity: 74.0 %
```

These are excerpts from the supplied serial logs. Repeated driver-level
phase-B messages are omitted from the disconnection excerpt.

## Assessment

PASS for the observed behavior:
- Missing DHT11 communication produced explicit errors.
- Failed reads did not print temperature or humidity measurements.
- Potentiometer monitoring continued.
- DHT11 measurements resumed after reconnection.

Limitations:
- Recovery excerpt does not show the first read after boot.
- Live reconnection and measurement accuracy were not tested.
- DRY confirms simulated moisture only, not overall sensor health.