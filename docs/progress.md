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
- Wi-Fi provisioning is planned; not implemented yet.
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

The shown partition table still has a 1 MB factory application partition;
selecting 16 MB flash does not enlarge that partition automatically.

## Slack delivery test

Created Plant Monitor Lab workspace, #plant-alerts channel,
and Plant Monitor app with incoming-webhook permission.

A PowerShell HTTPS POST returned "ok".
The matching test message appeared in Slack.

This verifies computer-to-Slack delivery, not ESP32 delivery.
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
Exact threshold boundaries remain separately untested.

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

ADC errors and skipped sample sequences are programmed to reset
the candidate streak; those paths remain separately untested.

## FreeRTOS architecture

- Sensor task owns hardware acquisition.
- Monitoring task owns classification and confirmation state.
- Queue copies sensor_reading_t values between tasks.
- Queue capacity: four samples.
- Each sample includes separate ADC and DHT11 error results.
- Sequence numbers allow detection of missing samples.
- Sensor task uses vTaskDelayUntil() with a five-second period.
- Configured task priorities: sensing 5, monitoring 4.
- Starting stack allocation: 4096 bytes per task.

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

## Remaining work

- Preserve redacted logs, wiring photos, screenshots, and demonstration video.
- Add unit tests and fault-path tests.
- Implement notification task and an additional signaling mechanism.
- Implement Wi-Fi provisioning, reconnection, and time synchronization.
- Add secure delivery through all four channels.
- Add notification cooldown, bounded retries, and service-error handling.
- Verify sensor plausibility, stale-data handling, and message formatting.
- Record static-analysis/compiler-warning results.
- Complete README, diagrams, technical report, and final demonstration.