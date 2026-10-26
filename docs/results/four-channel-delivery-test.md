# Matching four-channel delivery test

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T1, T4, T7 / matching recipients |
| Method | Serial timing plus four recipient screenshots |
| Acceptance check | One acquisition timestamp and the same values are visible in all four selected services. |
| Outcome and scope | Matching NORMAL event observed at 2026-10-26 05:23:26 UTC; a single event is not endurance evidence. |

## Event identity

All four recipient screenshots show the same captured event:

| Field | Value |
|---|---|
| Device | Plant-01 |
| Measured time | 2026-10-26 05:23:26 UTC |
| Reason | confirmed moisture state |
| State | NORMAL |
| Simulated moisture | 45% |
| Temperature | 30.0 C |
| Relative humidity | 72.0% |

This verifies the adapted channel set: Gmail, Telegram, Discord, and Slack. It does not claim WhatsApp or X delivery. Moisture is a potentiometer simulation.

## Procedure

1. Start with DRY already confirmed and network/time available.
2. Turn the potentiometer into the NORMAL range.
3. Wait for three consecutive matching state classifications.
4. Observe each service result without restarting the board.
5. Open each recipient application and capture the message with the same measurement timestamp.
6. Cover the sender email address in the Gmail capture while retaining the event contents.

The three NORMAL samples were 33%, 42%, and 45%. Confirmation requires the same classification, not identical numeric readings. The confirmed event carries the third sample, 45%.

## Device observations

Times below are serial-log uptime in milliseconds. Elapsed values are relative to NORMAL confirmation; they describe the sequential dispatch observed in this run, not independently measured recipient-delivery latency.

| Observation | Uptime, ms | After confirmation |
|---|---:|---:|
| NORMAL candidate 1/3 | 197502 | Before confirmation |
| NORMAL candidate 2/3 | 202502 | Before confirmation |
| NORMAL confirmed, candidate 3/3 | 207502 | 0.000 s |
| Slack HTTP 200 / accepted | 209392 | 1.890 s |
| Discord HTTP 200 / accepted | 212672 | 5.170 s |
| Telegram HTTP 200 | 215332 | 7.830 s |
| Telegram request accepted | 215342 | 7.840 s |
| Gmail post-DATA 250 / server acceptance | 221872 | 14.370 s |
| Gmail submission accepted after session close | 222182 | 14.680 s |

Sensor readings continued at 212502, 217502, 222502, and 227502 ms. The observed five-second acquisition interval continued during dispatch.

## Serial evidence

The following excerpts preserve the supplied log lines; intervening protocol and repeated sensor lines are omitted.

```text
I (197502) POT: Raw: 1369 | Simulated moisture: 33% | State: NORMAL
I (197502) MONITOR: Candidate: NORMAL | Count: 1/3
I (202502) POT: Raw: 1727 | Simulated moisture: 42% | State: NORMAL
I (202502) MONITOR: Candidate: NORMAL | Count: 2/3
I (207502) POT: Raw: 1873 | Simulated moisture: 45% | State: NORMAL
I (207502) MONITOR: Candidate: NORMAL | Count: 3/3
I (207502) MONITOR: Confirmed state: NORMAL
I (207502) DHT11: Temperature: 30.0 C | Humidity: 72.0 %
I (209392) SLACK: HTTP status: 200
I (209392) NOTIFY: Slack request accepted
I (212502) POT: Raw: 1875 | Simulated moisture: 45% | State: NORMAL
I (212672) DISCORD: HTTP status: 200
I (212672) NOTIFY: Discord request accepted
I (215332) TELEGRAM: HTTP status: 200
I (215342) NOTIFY: Telegram request accepted
I (217502) POT: Raw: 1869 | Simulated moisture: 45% | State: NORMAL
I (219942) GMAIL: SMTP reply: 235
I (220982) GMAIL: SMTP reply: 354
I (221872) GMAIL: SMTP reply: 250
I (221872) GMAIL: SMTP server accepted the email
I (222182) GMAIL: SMTP reply: 221
I (222182) NOTIFY: Gmail SMTP submission accepted
I (222502) POT: Raw: 1864 | Simulated moisture: 45% | State: NORMAL
I (227502) POT: Raw: 1870 | Simulated moisture: 45% | State: NORMAL
```

## Recipient evidence

### Gmail

![Gmail received the matching NORMAL event; sender address covered](final-gmail.png)

### Telegram

![Telegram received the matching NORMAL event](final-telegram.png)

### Discord

![Discord received the matching NORMAL event beneath an earlier DRY message](final-discord.png)

### Slack

![Slack received the matching NORMAL event beneath an earlier DRY message](final-slack.png)

The matching message is the NORMAL entry measured at 05:23:26 UTC. Discord and Slack also show an earlier DRY event measured at 05:20:11 UTC; that earlier event is not the subject of this test.

## Assessment and limits

PASS for the observed matching-event delivery through the four selected services. The recipient screenshots agree on device, measurement time, state, moisture, temperature, humidity, and reason. Serial results show service acceptance, while screenshots independently show recipient visibility.

This run does not establish prolonged reliability, retry/rate-limit behavior, delivery under network loss, physical soil calibration, or full laboratory acceptance. The excerpt does not identify the exact Git commit of the running firmware.
