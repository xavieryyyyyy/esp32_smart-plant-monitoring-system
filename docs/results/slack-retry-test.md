# Slack retry-policy tests

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

Recipient delivery after cleanup remains to be confirmed.

## Limitations
- These simulations did not exercise HTTP header parsing.
- Permanent errors, malformed/missing Retry-After, long waits,
  cross-message rate-limit deadlines, and ambiguous transport
  failures remain untested.
- Retries after ambiguous failures can produce duplicate messages.
- Events abandoned by the sender are not automatically requeued.