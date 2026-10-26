# Gmail SMTP delivery test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T6, T7 / Gmail integration |
| Method | Real SMTP session and inbox screenshot |
| Acceptance check | The server accepts the submission and a matching message appears in the inbox. |
| Outcome and scope | Both observed; final SMTP acceptance alone would not establish recipient delivery. |

## Initial issue
Local validation rejected an app password containing display spaces.
Removing the spaces produced the required 16-character value.

## Observed result
- Connected to smtp.gmail.com on port 465 using verified TLS.
- SMTP authentication succeeded: 235.
- Message accepted after DATA: 250.
- Session closed: 221.
- Email appeared in the inbox with matching measurements:
  - Measured: 2026-10-26 00:15:54 UTC
  - State: DRY
  - Simulated moisture: 26%
  - Temperature: 28 C
  - Relative humidity: 77%
- Sensor sampling continued every five seconds.

![Gmail delivery](gmail-delivery.png)

## Limitations
SMTP rejection, timeout, retry, and rate-limit handling remain
separately untested. Overall lab validation is not complete.

## Later shared-event comparison

The [four-channel delivery record](four-channel-delivery-test.md) compares one later event across all four selected recipients. It supplements this historical integration record without changing the event documented above.
