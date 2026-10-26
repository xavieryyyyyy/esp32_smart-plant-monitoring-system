# Sensor validation regression test

## Changes
- Reject ADC values outside 0–4095.
- Reject nonfinite temperature or humidity.
- Reject humidity outside 0–100%.
- Publish output values only after a successful, valid read.

## Hardware observations
- DRY confirmed at 12.513 s.
- NORMAL confirmed at 122.513 s.
- WET confirmed at 182.513 s.
- All four notification services accepted each state event.
- DHT11 timeouts occurred at 122.513 s and 352.513 s.
- Each timeout recovered on the next sample, about five seconds later.
- Potentiometer monitoring continued during both failures.

## Limitations
These observations verify normal operation and timeout handling.
Out-of-range and nonfinite-value rejection have not been tested.