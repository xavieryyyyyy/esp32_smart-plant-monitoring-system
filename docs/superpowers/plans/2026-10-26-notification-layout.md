# Notification layout and Philippine time implementation plan

> **For agentic workers:** Use executing-plans to implement this plan task by task. Work is staged separately and copied into the shared project after verification; do not commit or push for the builder.

**Goal:** Implement the approved four-service layouts using the original captured measurement time displayed in PhST.

**Architecture:** Pass a typed notification snapshot to the four existing senders. A small network-free formatting module owns PhST conversion, fallback text, structured JSON, and the HTML email body. Existing sender transport and monitoring policy remain in place.

**Tech stack:** ESP-IDF C, existing cJSON and mbedTLS; installed host GCC/Python for focused offline checks.

**Spec:** `docs/superpowers/specs/2026-10-26-notification-layout.md`.

## Global constraints

- No credential reads in the host tests, new dependency downloads, real sends, hardware uploads, or Git publishing.
- Never mutate the captured Unix timestamp or global timezone; keep monotonic timing and sender order unchanged.
- Keep the beginner-friendly comment style and tracked C formatting.
- Keep historical UTC screenshots/logs intact; distinguish the new layout's offline verification from real receipt.

## Review focus

- Midnight/month/year rollover and twelve-hour midnight/noon conversion must be correct.
- Failed DHT or unsynchronized acquisition must remain explicit on every channel, including email fallback.
- Small buffers, invalid values, and cJSON allocation failure must not emit truncated/partial messages or leak memory.
- Structured payloads must preserve the same fields and disable accidental Discord mentions.
- MIME alternatives must parse as plain text and HTML; sending still ends with exactly one SMTP DATA terminator.

## Tasks

- [x] Add failing host tests for time, payload structure, missing values, bounds, and memory cleanup; run before writing the formatter.
- [x] Add `notification_format.c/.h` and a typed snapshot in the public notification header; make the focused tests pass.
- [x] Adapt the four senders and `notification_task` to the snapshot; retain TLS/timeouts/retries and add Gmail multipart alternatives.
- [x] Verify MIME output and compile the application; render the generated email HTML for desktop/mobile inspection.
- [x] Update the README, file guide, test record and report; mark new recipient delivery as pending.
- [x] Review the final change, rerun affected checks, and hand the builder upload/test instructions.

## Execution notes

- Existing monitoring/sensing jobs and the six-case hardware suite are preserved. The new offline suite tests actual formatting code with real cJSON, not network mocks.
- Inter is a preferred email font and verified mockup font; native chat fonts and client font substitution are outside firmware control.

- Final review: no Critical/Important defects. Corrected the MIME type parameter with a failing/passing assertion. Actual provider rendering remains for builder-led hardware verification.
