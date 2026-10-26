# First ESP32-to-Slack delivery test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T1, T7 / Slack integration |
| Method | Historical real delivery and later timestamped message |
| Acceptance check | Accepted requests have matching visible Slack messages. |
| Outcome and scope | Observed for recorded events; the early duplicate cause remains unconfirmed. |

## Observed results
- Clock synchronized through SNTP.
- TLS log reported: Certificate validated.
- Slack returned HTTP 200.
- A message appeared in the Slack channel with matching values:
  Plant-01, DRY, simulated moisture 0%, temperature 29 C,
  relative humidity 75%.

![Slack delivery](slack-delivery.png)

## Limitations
- Screenshot contains two identical updates; whether these followed
  separate boots or occurred during one run is not yet established.
- Measurement timestamps were added and tested subsequently;
  see the measurement timestamp test below.
- At this first-delivery checkpoint, retry/rate handling and cooldown were not yet verified. Later evidence is in the [retry-policy record](slack-retry-test.md) and [cooldown record](notification-cooldown-test.md). Their stated limits still apply.
- Local webhook credentials are excluded from Git.

## NORMAL transition test

During the same running session:
- NORMAL candidate counts progressed from 1/3 to 3/3.
- State confirmed at 272501 ms.
- TLS certificate validation succeeded.
- Slack returned HTTP 200 at 274171 ms.
- One NORMAL message appeared with simulated moisture 42%,
  temperature 29 C, and humidity 75%.

The supplied excerpt ends after delivery. A full 30-second
no-repeat observation is not established by this excerpt.
The cause of the two earlier DRY messages remains unconfirmed.

![NORMAL delivery](slack-normal-delivery.png)


## Measurement timestamp test

A WET message arrived with measurement time
2026-10-25 11:08:07 UTC and matching values:
100% simulated moisture, 28 C, and 76% RH.

The supplied serial log showed certificate validation and HTTP 200.
Sampling continued every five seconds, with no additional send
shown for approximately 33 seconds after acceptance.

This verifies the synchronized-time path. The unsynchronized-time
fallback and delayed-queue timestamp preservation remain untested.

![Timestamped Slack message](slack-timestamp-delivery.png)

## Later shared-event comparison

The [four-channel delivery record](four-channel-delivery-test.md) compares one later event across all four selected recipients. It supplements this historical integration record without changing the event documented above.
