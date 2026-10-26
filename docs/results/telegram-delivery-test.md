# Telegram delivery test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T7 / Telegram integration |
| Method | Real bot API and chat screenshot |
| Acceptance check | Telegram accepts a message and matching fields appear in the intended chat. |
| Outcome and scope | Delivery observed; rejection, rate, and response-overflow cases remain open. |

## Observed result
Plant-01 message received in the bot chat:

- Measured: 2026-10-25 17:14:55 UTC
- State: DRY
- Simulated moisture: 26%
- Temperature: 28 C
- Relative humidity: 78%

Device logs reported HTTP 200, and the JSON ok-field check passed.
Sensor sampling continued every five seconds.

![Telegram delivery](telegram-delivery.png)

## Limitations
Retries, rate-limit handling, invalid credentials,
and oversized-response handling remain untested.

## Later shared-event comparison

The [four-channel delivery record](four-channel-delivery-test.md) compares one later event across all four selected recipients. It supplements this historical integration record without changing the event documented above.
