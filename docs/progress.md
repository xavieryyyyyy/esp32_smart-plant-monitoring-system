# Development Progress

This record was reconstructed from development conversations and
supplied terminal logs. It distinguishes observations from pending tests.

## Project decisions

- ESP32-S3 N16R8: 16 MB flash and 8 MB PSRAM.
- VS Code and PlatformIO with ESP-IDF; firmware written in C.
- DHT11 HW-481 V0.2 replaces the specified DHT22.
- Potentiometer temporarily simulates soil moisture.
- Selected channels: Gmail SMTP, Telegram, Discord, and Slack.
- Instructor permission to adapt the lab was reported by the builder.
- BLE Wi-Fi provisioning, saved-credential reconnection, and clock synchronization are implemented.
- ESP32 delivery through Gmail SMTP, Telegram, Discord, and Slack was verified.
- Per-condition notification cooldown defaults to 15 minutes.
- TLS certificate validity-date checking is enabled.
- PSRAM configuration and operation remain unverified.

## Current wiring

| Part connection | ESP32-S3 |
|---|---|
| Potentiometer middle terminal | GPIO1 |
| Potentiometer outer terminals | 3V3 and GND |
| DHT11 S | GPIO2 |
| DHT11 VCC | 3V3 |
| DHT11 minus | GND |

Earlier wiring plans used GPIO4 and GPIO13, then GPIO16 for DHT11.
The current firmware uses GPIO1 and GPIO2.

## Development milestones

| Commit | Milestone |
|---|---|
| 6c66fee | Initialized ESP-IDF project |
| a1ef481 | Read potentiometer on GPIO1 and aligned flash configuration |
| 38fdaab | Added DHT11 readings and startup initialization adjustment |
| d8932b1 | Converted ADC readings to simulated percentages and states |
| e90bb46 | Added three-reading state confirmation |
| c32d6a9 | Extracted conversion and classification into monitoring component |
| 7de0a2b | Moved confirmation state into monitoring component |
| 2a18d38 | Extracted hardware access into sensors component |
| 40bfc37 | Added sensor and monitoring tasks connected by a queue |
| a6509fe | Added BLE Wi-Fi provisioning and saved-credential reconnection |
| 8c21b7c | Documented device-triggered Wi-Fi reconnection |
| a78fe1e | Added verified Slack HTTPS delivery |
| ce6da8a | Added measurement timestamps to notifications |
| 88332e0 | Added bounded Slack retries and documented controlled tests |
| 31e3fa0 | Added verified Discord delivery |
| 6e9b5af | Added verified Telegram delivery and component-build refresh |
| 11a2918 | Added verified Gmail SMTP delivery |
| 08c1c6f | Added three passing monitoring unit tests |
| db3b173 | Added per-condition cooldown with six passing unit tests |
| 277b9ad | Enabled TLS certificate validity-date checks |
| a393270 | Added sensor-value checks and documented hardware regression results |

## Environment and flash configuration

Observed build environment:
- PlatformIO Espressif32 platform 7.1.2.
- ESP-IDF 6.1.0.
- Base board profile: esp32-s3-devkitc-1.
- Serial monitor: 115200 baud.

Changed ESP-IDF flash size from 2 MB to 16 MB using menuconfig.
Added board_upload.flash_size = 16MB to platformio.ini.
The flash mismatch warning disappeared.
A later boot log reported 16 MB flash.

A custom partitions.csv enlarges the factory application partition
to 4 MB. This resolved the firmware exceeding the original 1 MB limit.
The physical flash configuration remains 16 MB.

## Slack delivery test

Created Plant Monitor Lab workspace, #plant-alerts channel,
and Plant Monitor app with incoming-webhook permission.

A PowerShell HTTPS POST returned "ok".
The matching test message appeared in Slack.

This initial test verified computer-to-Slack delivery.
Later ESP32 HTTPS delivery returned HTTP 200, and matching messages
appeared in Slack. Measurement timestamps and bounded retries were added.

See [ESP32 delivery evidence](results/slack-delivery-test.md)
and [retry tests](results/slack-retry-test.md).

Webhook credentials must not appear in GitHub or evidence.

## Potentiometer testing

Raw ADC readings changed as the knob turned.

Observed stationary ranges:
- Position 1: 3345–3355.
- Position 2: 1814–1819 after settling.

No software smoothing was added.
Left/right direction was discussed but not explicitly confirmed.

Conversion:
simulated_percent = raw * 100 / 4095

Demonstration classifications:
- Below 30%: DRY.
- 30–70% inclusive: NORMAL.
- Above 70%: WET.

All three states were observed on hardware.
These values are simulated input levels, not calibrated soil moisture.
Unit tests passed for classification boundaries:
29% DRY, 30% NORMAL, 70% NORMAL, and 71% WET.

## DHT11 startup investigation

Initial behavior:
- First read failed with ESP_ERR_TIMEOUT in phase B.
- Subsequent readings succeeded.
- Pattern occurred after reset and USB power reconnection.

Investigation:
- Additional startup waiting did not resolve the timeout.
- Input pull-up configuration did not resolve it.
- Idle signal level measured by GPIO was HIGH.
- Presetting the GPIO output HIGH before direction setup
  was followed by successful first reads.
- Builder reported successful reset and power-cycle repeat tests.

Working initialization order:
1. gpio_set_level(GPIO_NUM_2, 1)
2. Configure GPIO2 as input.
3. Enable input pull-up.
4. Wait two seconds before sampling.

A brief unwanted LOW pulse during initial direction switching is a
hypothesis consistent with the observations. It was not confirmed
using an oscilloscope or logic analyzer.

## Monitoring confirmation

Three consecutive matching valid ADC readings confirm a state.
A different candidate restarts the count.
Confirmation is logged only when the confirmed state changes.

Observed on hardware:
- DRY, NORMAL, and WET confirmed after three readings.
- A two-reading interruption did not change the confirmed state.
- Holding a confirmed state did not repeat confirmation.

A unit test verified that resetting the streak requires three fresh
matching readings before confirmation.
Actual ADC-error and skipped-sequence paths remain separately untested.

## FreeRTOS architecture

- Sensor task owns hardware acquisition.
- Monitoring task owns classification and confirmation state.
- Queue copies sensor_reading_t values between tasks.
- Queue capacity: four samples.
- Each sample includes separate ADC and DHT11 error results.
- Sequence numbers allow detection of missing samples.
- Sensor task uses vTaskDelayUntil() with a five-second period.
- Notification task receives confirmed-state events through a second queue.
- Notification queue capacity: four events.
- Notification task sends requests sequentially to all four services.
- Configured task priorities: sensing 5, monitoring 4, notifications 3.
- Allocated stacks: sensing and monitoring 4096 bytes each;
  notifications 8192 bytes.
- Connectivity uses event-group flags and task notifications for signaling.
- Network requests run separately from sensor acquisition.

Observed logs were 5000 ms apart.
Both sensors and three-state confirmation continued operating.
No queue-full, watchdog, or crash errors appeared in the supplied run.

Queue overflow, stack margins, and extended reliability remain untested.

## DHT11 disconnection and recovery

See [test record](results/dht11-disconnection-test.md).

With the signal disconnected, four cycles reported DHT11 timeouts
while potentiometer processing continued.

After reconnection, two supplied samples showed 30 C and 74% RH.
The recovery excerpt does not establish first-read-after-boot behavior.

## Integration and test evidence

- [Wi-Fi reconnection](results/wifi-reconnection-test.md):
  device-triggered disconnect recovered automatically.
- [Slack delivery](results/slack-delivery-test.md):
  ESP32 messages received with measurement timestamps.
- [Slack retries](results/slack-retry-test.md):
  controlled failure tests verified bounded retry behavior.
- [Discord delivery](results/discord-delivery-test.md):
  notification received.
- [Telegram delivery](results/telegram-delivery-test.md):
  notification received.
- [Gmail delivery](results/gmail-delivery-test.md):
  email received.
- [Monitoring unit tests](results/monitoring-unit-tests.md):
  initial three tests passed; the expanded suite later passed six tests.
- [Notification cooldown](results/notification-cooldown-test.md):
  repeated DRY event suppressed while NORMAL recovery remained enabled.
- [TLS date checking](results/tls-date-validation-test.md):
  all four integrations worked after enabling certificate date checks.
- [Sensor validation regression](results/sensor-validation-test.md):
  three-state operation continued; intermittent DHT11 timeouts recovered.

## Remaining work

- Complete README, architecture and wiring diagrams, technical report,
  and demonstration video.
- Test rejection of out-of-range and nonfinite sensor values.
- Verify stale-data handling and message-formatting edge cases.
- Test skipped-sample handling, queue overflow, and task stack margins.
- Record static-analysis/compiler-warning results.
- Investigate intermittent DHT11 timeouts.
- Review retry and service-error handling for Discord, Telegram, and Gmail.
- Document how to replace saved Wi-Fi credentials.
- Preserve redacted logs, wiring photos, and screenshots.

Implemented features still have limits:
- Cooldown starts when an event is queued and resets after reboot.
- Slack retries are bounded; delivery retries may create duplicates.
- Router-outage recovery and invalid-date certificate rejection
  remain untested.
- Potentiometer percentages simulate moisture; they are not soil calibration.
- PSRAM configuration and operation remain unverified.