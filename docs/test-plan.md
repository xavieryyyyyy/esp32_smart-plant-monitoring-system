# Verification plan and channel-access checklist

[Rubric checklist](rubric-checklist.md) · [Recorded results](results/README.md) · [Main README](../README.md)

This plan separates completed observations from procedures that remain to be executed. Writing an acceptance criterion does not mark it passed. Hardware assumptions are ESP32-S3 N16R8, potentiometer on GPIO1, DHT11 on GPIO2, common 3V3/GND, and the four selected services.

## Baseline and evidence capture

- Open the project root containing `platformio.ini`; use `framework = espidf` and the tracked 16 MB flash / 4 MB app configuration.
- Record the source commit/worktree state, boot-reported app version, tool versions, test command, and whether temporary test code is present. A boot version alone does not prove an exact source-to-binary match.
- Keep credentials and BLE proof-of-possession codes outside public screenshots. Preserve original timestamps and measurement values when redacting personal fields.
- Distinguish service acceptance from visible receipt. Record one shared acquisition timestamp across all four outputs.
- Restore production firmware after Unity tests. Remove injected test triggers and verify cleanup before describing normal operation.
- Keep observed results with the applicable test record. Raw excerpts may omit unrelated lines, but the record must say so.

## T1–T8 acceptance plan

| Case | Procedure | Acceptance check | Current coverage |
|---|---|---|---|
| T1 Startup/sensing | Boot production firmware; inspect both sensors and time readiness over multiple samples | Correct units; periodic acquisition; valid time or an explicit unavailable label; no false DHT measurement after failure | Periodic operation and timestamps observed; DHT intermittently fails; failed-DHT/time display now covered by offline format checks; new layout hardware receipt pending |
| T2 States/calibration | Hold low, middle, high potentiometer inputs; compare raw/index/classification | DRY below 30%; NORMAL 30–70%; WET above 70%; confirm after three same-class samples | Bench states observed; real soil endpoints are not measured |
| T3 Invalid input | Exercise missing DHT, driver errors, nonfinite/out-of-range inputs, and stale timestamps in controlled tests | Reject/flag invalid data, avoid misleading values, reset confirmation when required | DHT failure and old timestamp observed; numerical rejection, actual ADC faults, future/exact-age boundary remain open |
| T4 Notification policy | Confirm DRY → NORMAL → DRY without reboot; exercise independent cooldown times | A real eligible event delivers; repeated DRY inside cooldown is suppressed; NORMAL remains eligible | Hardware suppression plus six logic cases recorded |
| T5 Connectivity loss | Trigger a controlled device disconnect; record acquisition and reconnection throughout | Loss detected; local monitoring continues appropriately; connection restored; outcome logged | Device disconnect recovery recorded; supplied disconnected-interval sensing proof and prolonged/router-outage cases incomplete |
| T6 Service failure/rate limit | Inject bounded failures for unit logic, then exercise safe real service responses where practical | Expected error recorded, attempt/wait limits respected, no infinite request loop | Slack 503/429 injection observed; real header parser and other-channel error matrices incomplete |
| T7 Four recipients | Confirm one event after network/time readiness; capture serial and each recipient | Same device ID, measured time, reason, state, and sensor fields visible in all four selected apps | Matching historical NORMAL event at 2026-10-26 05:23:26 UTC recorded; repeat for the new PhST/native layouts |
| T8 Restart/recovery | Repeat controlled resets/power cycles with saved credentials; inspect diagnostics | No hang/crash or secret disclosure; appropriate initial reporting and reconnection | Reuse/startup observations exist; systematic repeated-cycle/endurance experiment remains open |

Do not power off or change the shared boarding-house router. A broader network-loss experiment needs a controlled network whose owner permits the test. The existing device-disconnect test is deliberately narrower.

## Unit-test coverage

Command: `pio test -f test_monitoring`. This uploads test firmware. Restore with `pio run -t upload` afterward.

| Logic | Existing test/evidence | Next acceptance check |
|---|---|---|
| Raw ADC conversion | No dedicated test | Verify valid endpoints and a middle input: 0 → 0%, 2048 → 50%, 4095 → 100%; compare integer truncation |
| Classification | Boundary test passed | Preserve 29% DRY, 30%/70% NORMAL, 71% WET |
| Persistence | Third-match/no-repeat and reset tests passed | Add different-candidate interruption and sequence-gap path coverage |
| Cooldown | Three cooldown cases passed using synthetic time | Add invalid-state/time boundary cases if the interface contract requires them; preserve the hardware suppression observation |
| Message formatting | [17-case host suite and simulated SMTP/MIME checks](results/notification-formatting-test.md) | Verify real recipient rendering after upload; retain offline bounds, fallback, captured-time and allocation-failure checks |
| Sensor validation | Checks implemented; normal regression only | Inject NaN/infinity, humidity outside 0–100%, ADC outside 0–4095, and driver failures; verify caller outputs are not published on failure |

The conversion helper does not itself enforce a physical connection or calibration. A validation test should not assume that a disconnected analog input necessarily produces an out-of-range value.

## Reliability and analysis still to add

| Check | Evidence to capture | Current state |
|---|---|---|
| Reading/event queue pressure | Trigger and log capacity pressure; verify documented drops and confirmation behavior | Not experimentally established |
| Stack headroom | Minimum remaining stack under representative sensing and dispatch load | Allocations documented; margins unmeasured |
| Endurance | Defined run duration, input/network conditions, reset/error counts | No bounded endurance experiment recorded |
| Custom-component analysis | Tool/version, actual analyzed paths, findings, corrections | Current Cppcheck report identifies `src` only |
| Clean-clone build | New checkout, sanitized config procedure, exact resolved versions and build outcome | Not separately verified |
| Certificate rejection | Controlled invalid-date/trust case that retains verification | Date configuration enabled; rejection experiment absent |

## Channel-access checklist

This checklist records demonstrated access, not a promise that credentials will remain valid indefinitely. All private values belong in the ignored local header.

| Selected service | Configuration names | Setup/permission path | Evidence of working access |
|---|---|---|---|
| Gmail SMTP | `GMAIL_ADDRESS`, `GMAIL_APP_PASSWORD`, `GMAIL_RECIPIENT` | Eligible Google account/app password; authenticated SMTP over verified TLS | [Matching inbox event](results/final-gmail.png); post-DATA 250 and recipient visibility |
| Telegram | `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | BotFather token; authorized bot conversation/chat | [Matching bot message](results/final-telegram.png); HTTP 200 and `ok: true` |
| Discord | `DISCORD_WEBHOOK_URL` | Webhook for a channel under the builder's control; plain URL, firmware appends `wait=true` | [Matching channel event](results/final-discord.png); HTTP 200 |
| Slack | `SLACK_WEBHOOK_URL` | Workspace app with incoming-webhook permission authorized to the target channel | [Matching workspace event](results/final-slack.png); HTTP 200 |

No separate paid relay or OAuth server is configured. That implementation fact does not guarantee unlimited free provider access or future plan availability. The original WhatsApp/X requirements are documented in the [adaptation and grading matrix](rubric-checklist.md).

<a id="demonstration"></a>
## Short demonstration and face-to-face explanation

**Status: planned; video deferred by the builder.** The professor's actual demonstration acceptance has not been marked complete.

1. Show the real assembly and identify GPIO1, GPIO2, supply, and ground.
2. Show the five-second serial readings. Explain raw ADC, simulated percent, candidate count, confirmed state, air temperature, and RH.
3. Hold one new range for three samples and show one captured event in all four recipients with the same `Measured` timestamp.
4. Explain task separation and show acquisition continuing during dispatch.
5. Explain or demonstrate the existing cooldown suppression record without rebooting inside the test sequence.
6. State that soil moisture is simulated and that intermittent DHT timeouts and the recorded verification gaps remain.

A video file/link should be added only after recording. A face-to-face demonstration should be recorded as completed only after it occurs. This plan does not claim a waiver of any instructor-directed deliverable.

## Priority before final acceptance

The most direct unclosed checks are raw conversion, actual receipt of the new layouts, numeric validation, and wider reliability/error evidence. Preserve the current working firmware as a Git checkpoint before changing it; add one reviewed change and its actual test result at a time.


## New-layout hardware acceptance check

After uploading production firmware, wait for Wi-Fi and time readiness. Hold one state long enough for three readings, then compare the same measured PhST time and values in Gmail, Telegram, Slack, and Discord. Verify bold labels, fields, and the simulation note. Check Gmail's HTML card and, where available, its plain-text alternative. Native fonts, exact field wrapping, and app posting timestamps are client-controlled. Keep the original UTC evidence; save the new screenshots as a separate result only after actual receipt.
