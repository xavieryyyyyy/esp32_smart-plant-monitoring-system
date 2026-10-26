# Code guide: from a sensor reading to a notification

[Project README](../README.md#repository-map) · [Documentation guide](README.md) · [Technical report](technical-report.pdf) · [Results index](results/README.md)

The application has three main jobs: acquire measurements, decide whether a moisture state has changed, and deliver a snapshot of that event. These jobs live in separate FreeRTOS tasks so a network request does not occupy the acquisition task. A separate connectivity component supplies network and clock readiness.

## Start here

1. Read `src/main.c` for startup order. ESP-IDF invokes `app_main()`; it starts the application tasks and connectivity, then returns. The scheduler continues running the created tasks.
2. Read `components/sensors/include/sensors.h` for the sample structure passed between tasks.
3. Read `src/app_tasks.c` for both queues, all three application tasks, and message assembly.
4. Follow each included component header to its implementation below. Headers declare the interface; C files implement it; CMake declares which files and component dependencies enter the build.

## One complete data path

```text
GPIO1 / ADC1 channel 0       GPIO2 / DHT11
            \                 /
             components/sensors/sensors.c
                          |
                    sensor_task
                          | sensor_reading_t copied by value
                    reading_queue (4)
                          |
                    monitoring_task
                          | sequence + age checks
             components/monitoring/monitoring.c
                          | conversion -> class -> 3 readings
                          | changed state -> cooldown eligibility
                    notification_queue (4)
                          | copied event: reading + percent + state
                    notification_task
                          | format one message from that snapshot
            Slack -> Discord -> Telegram -> Gmail
                          | Wi-Fi + TLS to external services
                    recipient applications
```

BLE is used for first-time Wi-Fi setup from the phone. It is not the transport for these four Internet notifications. `connectivity.c` manages BLE provisioning, Wi-Fi events, reconnection, and SNTP. The senders query its public readiness functions before attempting delivery.

## File responsibilities and connections

Paths below are relative to the repository root. Each folder's `include` directory is the component's public interface.

| File | Responsibility | Called by / connected to |
|---|---|---|
| `src/main.c` | ESP-IDF entry point and startup order | Calls `app_tasks_start()` and `connectivity_start()` |
| `src/app_tasks.h` | Declares application startup | Included by `main.c` and `app_tasks.c` |
| `src/app_tasks.c` | Creates queues/tasks; acquires, processes, formats, dispatches | Calls sensors, monitoring, connectivity, and notification interfaces |
| `components/sensors/include/sensors.h` | Defines `sensor_reading_t` and acquisition functions | Shared by acquisition, monitoring, and notification snapshots |
| `components/sensors/sensors.c` | Owns ADC/DHT initialization and reads; validates returned measurements | Calls ESP-IDF ADC/GPIO and the managed DHT driver; returns separate error codes |
| `components/monitoring/include/monitoring.h` | Defines confirmation/cooldown state and its API | Used by the monitoring task and Unity tests |
| `components/monitoring/monitoring.c` | Converts the valid ADC index, classifies, confirms transitions, checks cooldown | Pure decision logic; does not read pins, send requests, or log delivery |
| `components/connectivity/include/connectivity.h` | Exposes startup, Wi-Fi readiness, and time readiness | Used by main, sensor timestamp capture, and all senders |
| `components/connectivity/connectivity.c` | BLE onboarding, saved credentials, Wi-Fi events, reconnect task, SNTP | Calls ESP-IDF networking/NVS and Espressif provisioning; maintains event-group flags |
| `components/notifications/include/notifications.h` | Declares the four synchronous sender functions | Included by `app_tasks.c` and sender implementations |
| `components/notifications/notifications.c` | Slack HTTPS JSON sender, bounded retries, rate-limit state | Called first by the notification task; uses connectivity and ESP HTTP client |
| `components/notifications/discord.c` | Discord webhook JSON with mentions disabled and `wait=true` | Called second; checks HTTP acceptance; one application-level attempt |
| `components/notifications/telegram.c` | Telegram `sendMessage`, bounded response collection, JSON `ok` check | Called third; bot token/chat ID come from private configuration |
| `components/notifications/gmail.c` | Gmail SMTP over TLS, authentication, MIME body, reply checks | Called fourth; submission success means SMTP acceptance |
| `components/notifications/config.example.h` | Public placeholder configuration template | Copy its macro names to the ignored local header; do not put credentials here |
| `components/notifications/config.local.h` | Private endpoint, token, chat, and email settings | Included locally by senders; ignored by Git; never publish it |
| `test/test_monitoring/test_main.c` | Six Unity cases for thresholds, persistence/reset, cooldown | Uses the monitoring component; uploads test firmware, not production tasks |
| `scripts/refresh_component_cmake.py` | PlatformIO pre-build hook for changed component declarations | Touches the root CMake file when needed to trigger reconfiguration; does not execute on ESP32 |

## Build and configuration files

| File | What it connects |
|---|---|
| `CMakeLists.txt` | Enters the ESP-IDF CMake build and names the project |
| `platformio.ini` | Selects ESP32-S3/ESP-IDF, flash and partition settings, monitor, tests, and the refresh hook |
| `src/CMakeLists.txt` | Compiles `main.c` and `app_tasks.c`; declares their component dependencies |
| `components/*/CMakeLists.txt` | Lists each component's C sources, exported include directory, and required components |
| `src/idf_component.yml` | Declares the managed DHT dependency |
| `components/connectivity/idf_component.yml` | Declares Espressif network provisioning |
| `components/notifications/idf_component.yml` | Declares managed cJSON for service payloads/responses |
| `dependencies.lock` | Records resolved managed-component versions and hashes; not a secret store |
| `sdkconfig.esp32-s3-devkitc-1` | Saved ESP-IDF options, including Bluetooth, flash, certificate bundle, and date validation |
| `partitions.csv` | Defines flash regions, including NVS and the 4 MB factory application slot |
| `.gitignore` | Keeps local secrets, generated build files, and selected editor/backup files out of Git |
| `docs/report-source/build_report.py` | Rebuilds `docs/technical-report.pdf` from Markdown and recorded figures |

An include declaration alone does not compile another C file. A new sender needs its public declaration, implementation, `SRCS` entry, required components, and application call. The refresh hook addresses stale configuration; it does not add missing sources automatically. `REQUIRES connectivity` makes the connectivity public header available to the notification component.

## Ownership, copies, and blocking calls

| Owner | State it owns | How another task receives it |
|---|---|---|
| Sensor task | ADC handle and acquisition sequence | A full sample is copied into `reading_queue` |
| Monitoring task | Candidate, confirmed state, count, DRY/WET cooldown timestamps | An eligible transition is copied into `notification_queue` |
| Notification task | Event snapshot, formatted message, single-caller sender state | Calls each sender synchronously; each sender releases its temporary resources |
| Connectivity component | Wi-Fi/time/provisioning flags, reconnect signaling | Readiness queries and ESP-IDF events; no shared measurement pointer |

Both queue sends use zero wait. Full queues drop new items and log the outcome. Sequence numbers allow the monitor to detect dropped readings. The notification queue has no delivery acknowledgement or persistent replay. Cooldown is recorded after successful enqueue, before external delivery; a failed delivery therefore still consumes the current cooldown interval.

Slack's static retry/rate state assumes the current single notification caller. Adding concurrent sender tasks would require reviewing that state and network/resource usage first. Existing stack allocations are starting sizes, not measured spare-stack guarantees.

## Two clocks with different purposes

- `sampled_at_us`: monotonic microseconds from `esp_timer_get_time()`, captured at acquisition start. The monitor uses it to reject negative age or age greater than ten seconds, independently of SNTP.
- `measured_at`: UTC wall-clock time captured at acquisition start only when synchronization is ready. The notification formats this stored value rather than substituting its later dispatch time.
- Cooldown also uses monotonic time. It is separate for DRY and WET, resets on reboot, and only filters confirmed state transitions; it does not schedule periodic reminders.

The healthy-DHT message has a `Measured` line or a clock-unavailable label. The current failed-DHT formatting branch omits that line. The freshness check applies before monitoring, not again before every network dispatch; old notification events can still wait behind earlier requests. These are recorded limitations, not behavior fixed by the comments.

## What happens on an error?

| Condition | Existing response | What it does not prove |
|---|---|---|
| ADC read/validation failure | Return an error; break the candidate streak; keep confirmed state | Physical disconnected analog input detection |
| DHT timeout/invalid value | Return an error without publishing partial output; valid ADC processing can continue | Sensor accuracy or a complete physical temperature plausibility check |
| Reading sequence gap | Reset the streak and process the received sample if otherwise valid | Recovery of the missing samples |
| Old/future sample timestamp | Reset the streak and discard the sample | Protection against every delayed notification |
| Full notification queue | Log a dropped event; do not record its cooldown | Automatic later delivery |
| Wi-Fi disconnect | Clear readiness and signal bounded-backoff reconnection | Recovery from every prolonged or changed-network scenario |
| One service fails | Log the result; continue to the next service | Guaranteed eventual receipt or exactly-once delivery |

## Reading the serial monitor during a short demonstration

| Example field/tag | Meaning |
|---|---|
| `I`, `W`, `E` | Information, warning, or error severity |
| `(207502)` | Milliseconds since boot for that log line; not a UTC timestamp |
| `POT: Raw` | The 12-bit ADC result from the potentiometer |
| `Simulated moisture` | Integer index from `raw * 100 / 4095`; not calibrated soil moisture |
| `State` | Classification of that individual valid sample |
| `Candidate / Count: 2/3` | Two consecutive samples in the same candidate class |
| `Confirmed state` | Three matching valid/fresh readings established a changed state |
| `DHT11` | Independent temperature/humidity result or acquisition error |
| `TIME` | Device clock synchronized; messages still show captured measurement time |
| `SLACK / DISCORD / TELEGRAM: HTTP status` | Service request outcome; check the recipient for visible delivery |
| `GMAIL: SMTP reply: 250` | Meaning depends on SMTP stage; final message acceptance is explicitly logged |
| `NOTIFY: ... accepted` | Sender reported service acceptance; not proof a person read it |
| `Cooldown suppressed` | The repeated DRY/WET transition is inside its own cooldown interval |

For the final check, compare the same device, measurement timestamp, state, and values across all four recipients. The recorded example is linked in the [four-channel delivery test](results/four-channel-delivery-test.md). The video remains deferred; the planned face-to-face demonstration has not been recorded as completed.
