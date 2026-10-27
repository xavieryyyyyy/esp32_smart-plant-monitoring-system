# Native notification layouts and Philippine time

[Results index](README.md) · [Code guide](../code-guide.md) · [Verification plan](../test-plan.md)

The later [responsive follow-up](responsive-notification-test.md) replaces the original paired fields with stacked layouts. This record describes the earlier formatting checkpoint.

## Scope

Review date: 26 October 2026. This verifies the new shared formatter and email assembly offline. The approved [design preview](../assets/notifications/design-preview.png) contains sample data and is not a recipient screenshot. Existing UTC/plain-text delivery evidence remains unchanged.

| Item | Result |
|---|---|
| Host compiler | GCC 14.2.0, installed CodeBlocks MinGW, C11; warnings treated as errors |
| Dependencies | Real managed cJSON source; only needed ESP result constants/time compatibility are stubbed |
| Formatting suite | 17 named cases passed |
| Host timezone check | Same 17 cases passed under default environment, UTC0 and PST8PDT; these are repeated environments, not 51 distinct cases |
| SMTP/MIME | Three full offline sender sessions passed: normal, unavailable DHT, unknown acquisition clock |
| Partial-write failure | Simulated network-write failure returned an error and released the connection |
| Firmware build | `pio run` passed in 81.50 s; flash 1,349,721 / 4,194,304 bytes; static RAM 42,048 / 327,680 bytes |
| Static analysis | `pio check --skip-packages` passed: 0 high, 0 medium, 2 existing low unused-function findings; reported scope remains src |
| Live new-layout receipt | Pending; no ESP32 upload or provider request was performed by these checks |

[Captured host-test output](notification-formatting-output.txt) is retained with this record.

## Reproduce

```powershell
python scripts/test_notification_format.py
```

An installed GCC-compatible compiler is required. Use `--cc "C:/path/to/gcc.exe"` to select one. The managed cJSON source must already be present from the normal firmware build. The runner does not download tools or read private configuration. Use `--output-dir PATH` to keep generated sample HTML, JSON and dummy SMTP captures for inspection.

The general formatter suite can run with host GCC. The full copied-sender SMTP harness additionally runs on Windows, using the operating system's Base64 encoder as the mbedTLS test stand-in. This does not test mbedTLS cryptography or a real TLS handshake.

## Cases and acceptance checks

- Captured Unix time `1792973754` displays as `26 October 2026, 8:15:54 AM PhST`; the original timestamp is unchanged.
- The +8-hour conversion handles year rollover and twelve-hour midnight/noon without changing the process timezone.
- Unknown acquisition time remains explicitly unavailable, even if a send later occurs after clock synchronization.
- Failed DHT values remain unavailable while valid moisture and measurement time are retained.
- Truncation, invalid state/percent/environment values, and null arguments are rejected. Partial text is cleared.
- Telegram supplies HTML parse mode and bold labels. Slack has native fields and complete fallback text. Discord has one native embed, state colors, and disabled mentions.
- Gmail contains the approved forest/status/readings/time/qualification structure, with Inter preference and system fallbacks.
- Each cJSON allocation point is exercised with an artificial allocation limit. Failed builds leave no partial payload or counted allocation leak.

## Email assembly checks

The test compiles an unchanged copy of `gmail.c` beside dummy settings. A fake SMTP peer supplies greeting/authentication/envelope/DATA/QUIT replies, including a multiline reply. Writes are restricted to seventeen bytes, exercising the actual partial-write loop. No socket connects to Gmail.

Python's MIME parser confirms an outer multipart/related containing a plain/HTML multipart/alternative and the inline PNG. The related root type is explicit, each decoded part carries the expected captured or unavailable values, Base64 lines are at most 76 characters, and SMTP DATA ends exactly once. The decoded image matches the included asset. A fresh review found the missing related `type` parameter; an assertion reproduced the issue before it was corrected. See [RFC 2387](https://www.rfc-editor.org/rfc/rfc2387).

Generated email HTML was rendered at 820 px and 390 px browser widths with its inline image resolved locally. Neither render had horizontal overflow. These are browser checks, not Gmail inbox screenshots. Native clients may substitute fonts or reflow fields.

## Preserved behavior and remaining checks

Executable-token comparison confirmed unchanged sensing, monitoring, task creation, and Slack retry policy. The formatter works on a copy of captured time; cooldown and freshness still use the monotonic timer. Send order, TLS configuration, credential handling, queue policy and existing service-acceptance checks remain in place.

Upload production firmware and capture one matching event in all four recipients before recording the new presentation as delivered. Check real font fallback, field wrapping, the inline mail image, and unavailable-data rendering. ESP32 heap/stack margins, prolonged failures, native application accessibility behavior, and end-to-end latency of the larger email remain unmeasured. The six monitoring tests retain their separate historical hardware result.
