# TLS certificate date-check configuration test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T1, T7 / TLS configuration |
| Method | Enabled date checks and successful synchronized requests |
| Acceptance check | Existing services still operate with certificate validity-date checking enabled. |
| Outcome and scope | All four integrations operated; rejection of invalid dates was not tested. |

## Configuration
CONFIG_MBEDTLS_HAVE_TIME_DATE=y

This enables certificate validity-date checks using the device clock.

## Hardware results
After rebuilding and uploading:
- Clock synchronized at 6.553 s.
- DRY state confirmed at 12.503 s.
- Slack, Discord, and Telegram returned HTTP 200.
- Gmail SMTP server accepted the email.

All four integrations continued working with date checking enabled.

## Limitations
Expired and not-yet-valid certificates were not tested.
Successful requests alone do not prove rejection of invalid dates.
