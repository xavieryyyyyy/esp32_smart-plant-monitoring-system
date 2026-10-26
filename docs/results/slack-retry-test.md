# Slack retry-policy tests

[Results index](README.md) · [Verification plan](../test-plan.md) · [Rubric checklist](../rubric-checklist.md)

## Test summary

| Field | Record |
|---|---|
| Coverage | T6 / retries and rate limits |
| Method | Injected status/delay values followed by a real cleanup request |
| Acceptance check | Attempts stop at three and injected delays are respected while sampling continues. |
| Outcome and scope | Selected retry paths observed; injection bypasses real header parsing. |

## Method
Temporary code injected response status and retry-delay values
inside slack_send_once(), bypassing network requests.

## Simulated HTTP 503
- Three attempts occurred.
- Delays were approximately 2.01 and 4.01 seconds.
- Sending stopped after attempt 3.
- Sensor sampling continued during retry waits.

## Simulated HTTP 429
- Injected Retry-After value: 3 seconds.
- Attempts occurred at 12469, 15499, and 18509 ms.
- Both intervals exceeded three seconds.
- Sending stopped after attempt 3.
- Sensor sampling continued.

## Cleanup verification
Removed the temporary simulation block and rebuilt/uploaded.

Observed:
- Attempt 1/3.
- TLS certificate validated.
- HTTP 200.
- Slack request accepted.
- Continued sensor sampling.

The cleanup excerpt establishes request acceptance, not visible receipt for that particular event. A later, separate [four-channel test](four-channel-delivery-test.md) includes matching Slack recipient evidence.

## Limitations
- These simulations did not exercise HTTP header parsing.
- Permanent errors, malformed/missing Retry-After, long waits,
  cross-message rate-limit deadlines, and ambiguous transport
  failures remain untested.
- Retries after ambiguous failures can produce duplicate messages.
- Events abandoned by the sender are not automatically requeued.
