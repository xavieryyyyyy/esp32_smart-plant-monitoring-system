# ESP32-to-Discord delivery test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T6, T7 / Discord integration |
| Method | Real service request and recipient screenshot |
| Acceptance check | A confirmed state is accepted by Discord and visible with matching fields. |
| Outcome and scope | Delivery observed; service-error and rate-limit cases remain open. |

## Initial failure
Discord HTTPS failed with:
"No matching trusted root certificate found."

Slack delivery and sensor sampling continued.

## Configuration change
Enabled certificate-bundle cross-signed verification.
Kept certificate verification enabled.

## Observed result
- Slack returned HTTP 200.
- Discord returned HTTP 200 with wait=true.
- Sensor sampling continued at five-second intervals.
- Discord displayed Plant-01 with:
  - Measured: 2026-10-25 15:17:47 UTC
  - State: NORMAL
  - Simulated moisture: 56%
  - Temperature: 28 C
  - Relative humidity: 78%

![Discord delivery](discord-delivery.png)

## Limitations
- Screenshot includes earlier messages from separate measurement times;
  their relationship to uploads or restarts was not established.
- Discord retries, rate-limit handling, and failure paths remain untested.
- Certificate-chain contents were not independently captured.

## Later shared-event comparison

The [four-channel delivery record](four-channel-delivery-test.md) compares one later event across all four selected recipients. It supplements this historical integration record without changing the event documented above.
