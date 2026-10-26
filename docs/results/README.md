# Test results and evidence index

[Documentation guide](../README.md) · [Rubric checklist](../rubric-checklist.md) · [Verification plan](../test-plan.md) · [Project README](../../README.md#verification)

## Strongest current observations

| Check | Record | What is established |
|---|---|---|
| Shared four-channel event | [Matching delivery](four-channel-delivery-test.md) | Same NORMAL event visible in Gmail, Telegram, Discord, Slack: 2026-10-26 05:23:26 UTC; 45%, 30 C, 72% RH |
| Message layout/time | [Offline formatting and MIME](notification-formatting-test.md) | 17 formatting cases; default plus two explicit host timezones; three simulated MIME sessions and a write-failure case; real receipt pending |
| Monitoring logic | [Six-test Unity run](monitoring-unit-tests.md) | Six recorded cases passed; screenshot and baseline retained |
| Static analysis | [Cppcheck 2.11 record](static-analysis.md), [raw output](static-analysis-output.txt) | Fresh check: 0 high, 0 medium, 2 low; reported component `src` |
| Cooldown | [Transition test](notification-cooldown-test.md) | Repeated DRY inside cooldown suppressed; NORMAL recovery allowed |
| Freshness | [Timestamp injection](stale-sample-test.md) | Old third sample rejected; confirmation required three subsequent fresh readings; test trigger removed |

## All records

| Record | Method | Laboratory cases | Principal limit |
|---|---|---|---|
| [Compiler build check](compiler-build-check.md) | Historical clean compiler build and warning search | QA/reproducibility | No warning matches is not proof of no defects |
| [DHT11 disconnection](dht11-disconnection-test.md) | Hardware failure and later recovery observations | T1, T3 | Does not establish live hot-plug recovery or accuracy |
| [Discord integration](discord-delivery-test.md) | Real external delivery, earlier TLS failure | T6, T7 | Historical event; retries/rate errors untested |
| [Matching four-channel delivery](four-channel-delivery-test.md) | Real recipient comparison and serial excerpts | T1, T4, T7 | Single captured event; adapted channel set |
| [Gmail integration](gmail-delivery-test.md) | Real SMTP and inbox delivery | T6, T7 | SMTP error/retry matrix incomplete |
| [Native layouts and PhST](notification-formatting-test.md) | Host GCC tests of real formatter and copied Gmail sender with dummy config | T1, T7 / formatting QA | No live new-layout delivery, ESP32 stack margin, or recipient rendering claimed |
| [Monitoring unit tests](monitoring-unit-tests.md) | Unity on ESP32-S3 | T4 plus logic QA | Conversion/formatting/numeric validation not covered |
| [Cooldown](notification-cooldown-test.md) | Hardware transitions plus separate synthetic-time tests | T4 | Starts at queue acceptance; resets on reboot |
| [Sensor validation regression](sensor-validation-test.md) | Normal hardware regression and intermittent DHT errors | T1, T3 | Invalid-number rejection not injected |
| [Slack integration](slack-delivery-test.md) | Historical actual recipient outputs | T1, T7 | Duplicate cause in early screenshot unconfirmed |
| [Slack retry policy](slack-retry-test.md) | Injected 503/429, then cleanup run | T6 | Injection bypassed HTTP header parsing |
| [Sample freshness](stale-sample-test.md) | Deliberately old timestamp, then cleanup | T3, T4 | Not a real queue-backlog experiment |
| [Static analysis](static-analysis.md) | Fresh Cppcheck 2.11 command output | QA | Two low findings; custom-component coverage not established |
| [Telegram integration](telegram-delivery-test.md) | Real bot/API delivery | T7 | Error/rate/overflow cases untested |
| [TLS date configuration](tls-date-validation-test.md) | Configuration plus real requests | T1, T7 / security | No expired/not-yet-valid certificate experiment |
| [Wi-Fi reconnection](wifi-reconnection-test.md) | Device-originated disconnect and cleanup | T5, T8 | Router outage, long failure, exact interval continuity unproven |

## Interpretation rules

A record's summary states the acceptance check and the scope of its outcome. The original observation text, serial excerpts, screenshot references, and limitations remain below it. Historical timestamps are not changed to the documentation review date.

Do not infer a passed whole-lab case from one narrow check. For example, an old-timestamp injection does not verify every invalid sensor path; configured stack sizes are not stack-margin measurements; service acceptance is not by itself a recipient screenshot. The full case coverage and unexecuted procedures are in the [verification plan](../test-plan.md).

The earlier three-test Unity screenshot remains historical; the newer six-test capture is the current suite evidence. Earlier single-service screenshots show different events, while the four `final-*.png` screenshots intentionally show one matching event.
