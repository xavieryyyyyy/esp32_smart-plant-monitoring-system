# Monitoring unit tests

Command: pio test -f test_monitoring
Framework: Unity
Target: ESP32-S3

## Result
3 test cases passed:
- Threshold boundaries: 29%, 30%, 70%, 71%.
- Confirmation on the third matching reading, without repeats.
- Explicit streak reset requires three new matching readings.

Total build/upload/test duration: 275.58 seconds.

The reset test calls monitoring_reset_streak() directly;
it does not simulate a physical ADC fault.

Cooldown behavior is not implemented or tested yet.

![Unity test results](monitoring-unit-tests.png)